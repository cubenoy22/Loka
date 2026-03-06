#include "context/ToolboxImageViewContext.hpp"
#include "ToolboxScenePlatformController.hpp"
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

  MoveTo(rect_.left, rect_.top);
  LineTo(rect_.right, rect_.bottom);
  MoveTo(rect_.right, rect_.top);
  LineTo(rect_.left, rect_.bottom);

  char label[64];
  std::sprintf(label, "Image: %dx%d", image_.width(), image_.height());
  DrawPascalStringAt(static_cast<short>(rect_.left + 6), static_cast<short>(rect_.top + 14), label);
}
