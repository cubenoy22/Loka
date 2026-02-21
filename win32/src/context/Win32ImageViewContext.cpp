#include "Win32ImageViewContext.hpp"

namespace
{
  const char *kImageViewClassName = "LOKA_IMAGE_VIEW";
}

Win32ImageViewContext::Win32ImageViewContext(HWND parent, int x, int y, int width, int height, loka::app::ImageViewNode *node)
    : node_(node),
      hwnd_(0),
      imageState_(0),
      image_()
{
  EnsureClassRegistered();
  hwnd_ = CreateWindowExA(
      0,
      kImageViewClassName,
      "",
      WS_CHILD | WS_VISIBLE,
      x,
      y,
      width,
      height,
      parent,
      0,
      GetModuleHandle(NULL),
      this);
  bindImage();
}

Win32ImageViewContext::~Win32ImageViewContext()
{
  unbindImage();
  if (hwnd_)
  {
    DestroyWindow(hwnd_);
    hwnd_ = 0;
  }
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
  Win32ImageViewContext *self = static_cast<Win32ImageViewContext *>(reinterpret_cast<void *>(GetWindowLongPtr(hwnd, GWLP_USERDATA)));
  if (msg == WM_NCCREATE)
  {
    CREATESTRUCT *cs = reinterpret_cast<CREATESTRUCT *>(lParam);
    self = static_cast<Win32ImageViewContext *>(cs->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }
  switch (msg)
  {
  case WM_ERASEBKGND:
    return 1;
  case WM_PAINT:
  {
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
    InvalidateRect(hwnd_, NULL, TRUE);
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
    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchBlt(hdc, rect.left, rect.top, dstWidth, dstHeight, memdc, 0, 0, srcWidth, srcHeight, SRCCOPY);
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
