#include "Win32ImageViewContext.hpp"
#include "../Win32ScenePlatformController.hpp"
#include "app/scene/projection/PlatformNodeHandler.hpp"

namespace
{
  const char *kImageViewClassName = "LOKA_IMAGE_VIEW";
  const int kVerticalSpacing = 12;

  class Win32ImageViewNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::ImageViewNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      loka::app::ImageViewNode *image = node ? node->asImageViewNode() : 0;
      Win32ScenePlatformController *win32 = static_cast<Win32ScenePlatformController *>(controller);
      if (!image || !win32)
      {
        return 0;
      }
      return win32->contextMapper()->ensureImageViewContext(image, state.x, state.y, state.width, state.height);
    }
  };

  Win32ImageViewNodeHandler gWin32ImageViewNodeHandler;

  struct BlitRect
  {
    int dstX;
    int dstY;
    int dstW;
    int dstH;
    int srcX;
    int srcY;
    int srcW;
    int srcH;
  };

  static BlitRect ComputeBlitRect(int fitMode, const RECT &rect, int srcWidth, int srcHeight)
  {
    BlitRect out;
    out.dstX = rect.left;
    out.dstY = rect.top;
    out.dstW = rect.right - rect.left;
    out.dstH = rect.bottom - rect.top;
    out.srcX = 0;
    out.srcY = 0;
    out.srcW = srcWidth;
    out.srcH = srcHeight;

    if (srcWidth <= 0 || srcHeight <= 0 || out.dstW <= 0 || out.dstH <= 0)
    {
      return out;
    }

    if (fitMode == loka::app::IMAGE_FIT_STRETCH)
    {
      return out;
    }

    if (fitMode == loka::app::IMAGE_FIT_NONE)
    {
      out.dstW = srcWidth;
      out.dstH = srcHeight;
      return out;
    }

    const double srcAspect = static_cast<double>(srcWidth) / static_cast<double>(srcHeight);
    const double dstAspect = static_cast<double>(out.dstW) / static_cast<double>(out.dstH);

    if (fitMode == loka::app::IMAGE_FIT_CONTAIN)
    {
      if (srcAspect > dstAspect)
      {
        out.dstH = static_cast<int>(out.dstW / srcAspect);
      }
      else
      {
        out.dstW = static_cast<int>(out.dstH * srcAspect);
      }
      out.dstX = rect.left + ((rect.right - rect.left) - out.dstW) / 2;
      out.dstY = rect.top + ((rect.bottom - rect.top) - out.dstH) / 2;
      return out;
    }

    if (fitMode == loka::app::IMAGE_FIT_COVER)
    {
      if (srcAspect > dstAspect)
      {
        const int cropWidth = static_cast<int>(srcHeight * dstAspect);
        out.srcX = (srcWidth - cropWidth) / 2;
        out.srcW = cropWidth;
      }
      else
      {
        const int cropHeight = static_cast<int>(srcWidth / dstAspect);
        out.srcY = (srcHeight - cropHeight) / 2;
        out.srcH = cropHeight;
      }
      return out;
    }

    return out;
  }

  int ResolveImageLayoutWidth(const loka::app::ImageViewNode *node, int fallbackWidth)
  {
    if (!node)
    {
      return fallbackWidth;
    }
    int sizePolicy = loka::app::IMAGE_VIEW_SIZE_AUTO;
    if (node->props.hasAttr_ && node->props.attr_.hasSizePolicyValue_)
    {
      sizePolicy = static_cast<int>(node->props.attr_.sizePolicyValue_);
    }
    const bool hasExplicitWidth = node->props.width_ > 0;
    int srcWidth = 0;
    if (node->props.image_)
    {
      const loka::core::resource::Image current = node->props.image_->get();
      srcWidth = current.width();
    }
    if (hasExplicitWidth)
    {
      return node->props.width_;
    }
    if (sizePolicy == loka::app::IMAGE_VIEW_SIZE_INTRINSIC && srcWidth > 0)
    {
      return srcWidth;
    }
    if (sizePolicy == loka::app::IMAGE_VIEW_SIZE_FILL_PARENT)
    {
      return fallbackWidth;
    }
    return fallbackWidth;
  }

  int ResolveImageLayoutHeight(const loka::app::ImageViewNode *node, int resolvedWidth, int fallbackHeight)
  {
    if (!node)
    {
      return fallbackHeight > 0 ? fallbackHeight : 160;
    }
    int sizePolicy = loka::app::IMAGE_VIEW_SIZE_AUTO;
    if (node->props.hasAttr_ && node->props.attr_.hasSizePolicyValue_)
    {
      sizePolicy = static_cast<int>(node->props.attr_.sizePolicyValue_);
    }
    const bool hasExplicitHeight = node->props.height_ > 0;
    int srcWidth = 0;
    int srcHeight = 0;
    if (node->props.image_)
    {
      const loka::core::resource::Image current = node->props.image_->get();
      srcWidth = current.width();
      srcHeight = current.height();
    }
    if (hasExplicitHeight)
    {
      return node->props.height_;
    }
    if (sizePolicy == loka::app::IMAGE_VIEW_SIZE_FILL_PARENT && fallbackHeight > 0)
    {
      return fallbackHeight;
    }
    if (srcWidth > 0 && srcHeight > 0 && resolvedWidth > 0)
    {
      return (resolvedWidth * srcHeight) / srcWidth;
    }
    if (srcHeight > 0)
    {
      return srcHeight;
    }
    if (fallbackHeight > 0)
    {
      return fallbackHeight;
    }
    return 160;
  }
} // namespace

Win32ImageViewContext::Win32ImageViewContext(
    HWND parent, int x, int y, int width, int height, loka::app::ImageViewNode *node)
    : node_(node),
      hwnd_(0),
      imageState_(0),
      image_()
{
  EnsureClassRegistered();
  hwnd_ = CreateWindowExA(
      0, kImageViewClassName, "", WS_CHILD | WS_VISIBLE, x, y, width, height, parent, 0, GetModuleHandle(NULL), this);
  bindImage();
}

Win32ImageViewContext::~Win32ImageViewContext()
{
  unbindImage();
  if (hwnd_)
  {
    SetWindowLongPtr(hwnd_, GWLP_USERDATA, 0);
    DestroyWindow(hwnd_);
    hwnd_ = 0;
  }
}

void Win32ImageViewContext::onNodeAttached()
{
  if (hwnd_)
  {
    ShowWindow(hwnd_, SW_SHOW);
  }
}

void Win32ImageViewContext::onNodeDetached()
{
  if (hwnd_)
  {
    ShowWindow(hwnd_, SW_HIDE);
  }
}

short Win32ImageViewContext::layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state)
{
  const int imageWidth = ResolveImageLayoutWidth(this->node_, state.width);
  const int imageHeight = ResolveImageLayoutHeight(this->node_, imageWidth, state.height);
  this->relayout(state.x, state.y, imageWidth, imageHeight);
  state.width = static_cast<short>(imageWidth);
  state.height = static_cast<short>(imageHeight);
  return static_cast<short>(state.y + imageHeight + kVerticalSpacing);
}

void Win32ImageViewContext::relayout(int x, int y, int width, int height)
{
  if (!hwnd_)
  {
    return;
  }
  MoveWindow(hwnd_, x, y, width, height, TRUE);
  Win32ScenePlatformController::requestDirtyRect(hwnd_, NULL, TRUE);
}

void Win32ImageViewContext::EnsureClassRegistered()
{
  static bool registered = false;
  if (registered)
  {
    return;
  }
  WNDCLASSA wc;
  ZeroMemory(&wc, sizeof(wc));
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = Win32ImageViewContext::WndProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = NULL;
  wc.lpszClassName = kImageViewClassName;
  RegisterClassA(&wc);
  registered = true;
}

LRESULT CALLBACK Win32ImageViewContext::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  Win32ImageViewContext *self =
      static_cast<Win32ImageViewContext *>(reinterpret_cast<void *>(GetWindowLongPtr(hwnd, GWLP_USERDATA)));
  if (msg == WM_NCCREATE)
  {
    CREATESTRUCT *cs = reinterpret_cast<CREATESTRUCT *>(lParam);
    self = static_cast<Win32ImageViewContext *>(cs->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }
  switch (msg)
  {
  case WM_ERASEBKGND:
    Win32ScenePlatformController::noteNativePaint(hwnd, Win32ScenePlatformController::NATIVE_PAINT_IMAGE, true);
    return 1;
  case WM_PAINT:
  {
    Win32ScenePlatformController::noteNativePaint(hwnd, Win32ScenePlatformController::NATIVE_PAINT_IMAGE, false);
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    if (self)
    {
      RECT rect;
      GetClientRect(hwnd, &rect);
      self->drawImage(hdc, rect);
    }
    EndPaint(hwnd, &ps);
    return 0;
  }
  default:
    break;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

void Win32ImageViewContext::bindImage()
{
  if (!node_)
  {
    return;
  }
  imageState_ = node_->props.image_;
  if (imageState_)
  {
    imageState_->deferBind(&Win32ImageViewContext::ImageChangedThunk, this);
    applyImage();
  }
}

void Win32ImageViewContext::unbindImage()
{
  if (imageState_)
  {
    imageState_->deferUnbind(&Win32ImageViewContext::ImageChangedThunk, this);
    imageState_ = 0;
  }
}

void Win32ImageViewContext::applyImage()
{
  if (!imageState_)
  {
    return;
  }
  image_ = imageState_->get();
  if (hwnd_)
  {
    Win32ScenePlatformController::requestDirtyRect(hwnd_, NULL, TRUE);
  }
}

void Win32ImageViewContext::drawImage(HDC hdc, const RECT &rect)
{
  HBRUSH fill = CreateSolidBrush(RGB(240, 240, 240));
  FillRect(hdc, &rect, fill);
  DeleteObject(fill);

  if (!image_.isValid())
  {
    return;
  }

  HBITMAP bitmap = static_cast<HBITMAP>(image_.nativeHandle());
  if (!bitmap)
  {
    return;
  }

  HDC memdc = CreateCompatibleDC(hdc);
  if (!memdc)
  {
    return;
  }
  HGDIOBJ old = SelectObject(memdc, bitmap);
  int dstWidth = rect.right - rect.left;
  int dstHeight = rect.bottom - rect.top;
  int srcWidth = image_.width();
  int srcHeight = image_.height();
  if (srcWidth > 0 && srcHeight > 0 && dstWidth > 0 && dstHeight > 0)
  {
    int fitMode = loka::app::IMAGE_FIT_STRETCH;
    if (node_ && node_->props.hasAttr_ && node_->props.attr_.hasFitValue_)
    {
      fitMode = static_cast<int>(node_->props.attr_.fitValue_);
    }
    BlitRect blit = ComputeBlitRect(fitMode, rect, srcWidth, srcHeight);
    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchBlt(
        hdc, blit.dstX, blit.dstY, blit.dstW, blit.dstH, memdc, blit.srcX, blit.srcY, blit.srcW, blit.srcH, SRCCOPY);
  }
  SelectObject(memdc, old);
  DeleteDC(memdc);
}

void Win32ImageViewContext::ImageChangedThunk(void *userData)
{
  Win32ImageViewContext *self = static_cast<Win32ImageViewContext *>(userData);
  if (self)
  {
    self->applyImage();
  }
}

void RegisterWin32ImageViewNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  registry.registerHandler(&gWin32ImageViewNodeHandler);
}
