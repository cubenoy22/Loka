#include "context/ToolboxImageViewContext.hpp"
#include "ToolboxScenePlatformController.hpp"
#include "ToolboxNativeImage.hpp"
#include "loka/platform/StringUTF8.hpp"
#include <cstdio>
#include <cstring>
#include <string>

namespace
{
  static void DrawPascalStringAt(short x, short y, const char *utf8)
  {
    if (!utf8)
    {
      return;
    }
    const std::size_t n = std::strlen(utf8);
    const std::size_t capped = (n > 255) ? 255 : n;
    Str255 text;
    text[0] = static_cast<unsigned char>(capped);
    if (capped > 0)
    {
      std::memcpy(text + 1, utf8, capped);
    }
    MoveTo(x, y);
    DrawString(text);
  }

  static Rect ComputeImageDrawRect(const Rect &frameRect,
                                   int fitMode,
                                   int imageWidth,
                                   int imageHeight)
  {
    Rect out = frameRect;
    const int frameWidth = frameRect.right - frameRect.left;
    const int frameHeight = frameRect.bottom - frameRect.top;
    if (imageWidth <= 0 || imageHeight <= 0 || frameWidth <= 0 || frameHeight <= 0)
    {
      return out;
    }

    if (fitMode == loka::app::IMAGE_FIT_NONE)
    {
      out.right = static_cast<short>(out.left + imageWidth);
      out.bottom = static_cast<short>(out.top + imageHeight);
      return out;
    }

    if (fitMode == loka::app::IMAGE_FIT_STRETCH)
    {
      return out;
    }

    // contain / cover
    const double sx = static_cast<double>(frameWidth) / static_cast<double>(imageWidth);
    const double sy = static_cast<double>(frameHeight) / static_cast<double>(imageHeight);
    const double scale = (fitMode == loka::app::IMAGE_FIT_COVER)
                             ? ((sx > sy) ? sx : sy)
                             : ((sx < sy) ? sx : sy);

    const int dstW = static_cast<int>(imageWidth * scale);
    const int dstH = static_cast<int>(imageHeight * scale);
    const int offsetX = (frameWidth - dstW) / 2;
    const int offsetY = (frameHeight - dstH) / 2;
    out.left = static_cast<short>(frameRect.left + offsetX);
    out.top = static_cast<short>(frameRect.top + offsetY);
    out.right = static_cast<short>(out.left + dstW);
    out.bottom = static_cast<short>(out.top + dstH);
    return out;
  }
}

ToolboxImageViewContext::ToolboxImageViewContext(loka::app::ImageViewNode *node)
    : node_(node),
      boundary_(0),
      rect_(),
      image_()
{
  SetRect(&rect_, 0, 0, 0, 0);
}

ToolboxImageViewContext::~ToolboxImageViewContext()
{
}

short ToolboxImageViewContext::layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state)
{
  int sizePolicy = loka::app::IMAGE_VIEW_SIZE_AUTO;
  int width = state.width;
  int height = state.lineHeight > 0 ? state.lineHeight : 80;
  if (node_)
  {
    const bool hasExplicitWidth = node_->props.width_ > 0;
    const bool hasExplicitHeight = node_->props.height_ > 0;
    if (node_->props.hasAttr_ && node_->props.attr_.hasSizePolicyValue_)
    {
      sizePolicy = static_cast<int>(node_->props.attr_.sizePolicyValue_);
    }

    if (hasExplicitWidth)
    {
      width = node_->props.width_;
    }
    if (hasExplicitHeight)
    {
      height = node_->props.height_;
    }

    int srcWidth = 0;
    int srcHeight = 0;
    if (node_->props.image_)
    {
      const loka::core::resource::Image current = node_->props.image_->get();
      srcWidth = current.width();
      srcHeight = current.height();
    }

    if (sizePolicy == loka::app::IMAGE_VIEW_SIZE_INTRINSIC && !hasExplicitWidth && srcWidth > 0)
    {
      width = srcWidth;
    }
    else if (sizePolicy == loka::app::IMAGE_VIEW_SIZE_FILL_PARENT && !hasExplicitWidth)
    {
      width = state.width;
    }

    if (!hasExplicitHeight)
    {
      if (sizePolicy == loka::app::IMAGE_VIEW_SIZE_FILL_PARENT && state.height > 0)
      {
        height = state.height;
      }
      else if (srcWidth > 0 && srcHeight > 0 && width > 0)
      {
        height = (width * srcHeight) / srcWidth;
      }
      else if (srcHeight > 0)
      {
        height = srcHeight;
      }
    }
  }
  updateRect(state.x, state.y, static_cast<short>(width), static_cast<short>(height));
  state.y = static_cast<short>(state.y + static_cast<short>(height) + state.spacing);
  return static_cast<short>(width);
}

void ToolboxImageViewContext::render(loka::app::scene::IPlatformController *)
{
  draw();
}

void ToolboxImageViewContext::updateRect(short x, short y, short width, short height)
{
  if (width < 0)
  {
    width = 0;
  }
  if (height < 0)
  {
    height = 0;
  }
  rect_.left = x;
  rect_.top = y;
  rect_.right = static_cast<short>(x + width);
  rect_.bottom = static_cast<short>(y + height);
}

void ToolboxImageViewContext::draw()
{
  EraseRect(&rect_);
  FrameRect(&rect_);

  if (!node_ || !node_->props.image_)
  {
    DrawPascalStringAt(static_cast<short>(rect_.left + 6), static_cast<short>(rect_.top + 14), "Image: (no state)");
    return;
  }

  image_ = node_->props.image_->get();
  if (!image_.isValid())
  {
    DrawPascalStringAt(static_cast<short>(rect_.left + 6), static_cast<short>(rect_.top + 14), "Image: (empty)");
    return;
  }

  int fitMode = loka::app::IMAGE_FIT_STRETCH;
  if (node_->props.hasAttr_ && node_->props.attr_.hasFitValue_)
  {
    fitMode = static_cast<int>(node_->props.attr_.fitValue_);
  }

  const loka::toolbox::ToolboxNativeImage *native =
      loka::toolbox::TryGetToolboxNativeImage(image_);
  if (native &&
      native->kind == loka::toolbox::TOOLBOX_NATIVE_IMAGE_KIND_PICT &&
      native->payload)
  {
    PicHandle picture = static_cast<PicHandle>(native->payload);
    Rect dstRect = ComputeImageDrawRect(rect_, fitMode, image_.width(), image_.height());
    if (fitMode == loka::app::IMAGE_FIT_COVER)
    {
      RgnHandle oldClip = NewRgn();
      if (oldClip)
      {
        GetClip(oldClip);
        ClipRect(&rect_);
        DrawPicture(picture, &dstRect);
        SetClip(oldClip);
        DisposeRgn(oldClip);
      }
      else
      {
        DrawPicture(picture, &dstRect);
      }
    }
    else
    {
      DrawPicture(picture, &dstRect);
    }
    return;
  }

  MoveTo(rect_.left, rect_.top);
  LineTo(rect_.right, rect_.bottom);
  MoveTo(rect_.right, rect_.top);
  LineTo(rect_.left, rect_.bottom);

  char label[64];
  std::sprintf(label, "Image(native?): %dx%d", image_.width(), image_.height());
  DrawPascalStringAt(static_cast<short>(rect_.left + 6), static_cast<short>(rect_.top + 14), label);
}
