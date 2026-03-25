#include "Win32RectSurfaceContext.hpp"
#include "../Win32ScenePlatformController.hpp"
#include "app/RectSurface.hpp"

namespace
{
  const char *kRectSurfaceClassName = "LOKA_RECT_SURFACE";
}

Win32RectSurfaceContext::Win32RectSurfaceContext(HWND parent,
                                                 int x,
                                                 int y,
                                                 int width,
                                                 int height,
                                                 loka::app::RectSurfaceNode *node)
    : node_(node),
      hwnd_(0)
{
  EnsureClassRegistered();
  hwnd_ = CreateWindowExA(0,
                          kRectSurfaceClassName,
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
}

Win32RectSurfaceContext::~Win32RectSurfaceContext()
{
  if (hwnd_)
  {
    DestroyWindow(hwnd_);
    hwnd_ = 0;
  }
}

void Win32RectSurfaceContext::relayout(int x, int y, int width, int height)
{
  if (!hwnd_)
  {
    return;
  }
  MoveWindow(hwnd_, x, y, width, height, TRUE);
  HWND parent = GetParent(hwnd_);
  if (parent)
  {
    RECT rect;
    if (GetWindowRect(hwnd_, &rect))
    {
      MapWindowPoints(NULL, parent, reinterpret_cast<POINT *>(&rect), 2);
      Win32ScenePlatformController::redrawDirtySubtreeNow(parent, &rect, TRUE);
    }
  }
}

void Win32RectSurfaceContext::EnsureClassRegistered()
{
  static bool registered = false;
  if (registered)
  {
    return;
  }
  WNDCLASSA wc;
  ZeroMemory(&wc, sizeof(wc));
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = Win32RectSurfaceContext::WndProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = NULL;
  wc.lpszClassName = kRectSurfaceClassName;
  RegisterClassA(&wc);
  registered = true;
}

LRESULT CALLBACK Win32RectSurfaceContext::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  Win32RectSurfaceContext *self = static_cast<Win32RectSurfaceContext *>(
      reinterpret_cast<void *>(GetWindowLongPtr(hwnd, GWLP_USERDATA)));
  if (msg == WM_NCCREATE)
  {
    CREATESTRUCT *create = reinterpret_cast<CREATESTRUCT *>(lParam);
    self = static_cast<Win32RectSurfaceContext *>(create->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }

  switch (msg)
  {
  case WM_ERASEBKGND:
    return 1;
  case WM_PAINT:
  {
    PAINTSTRUCT paint;
    HDC hdc = BeginPaint(hwnd, &paint);
    if (self)
    {
      RECT rect;
      GetClientRect(hwnd, &rect);
      self->draw(hdc, rect);
    }
    EndPaint(hwnd, &paint);
    return 0;
  }
  default:
    break;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

void Win32RectSurfaceContext::draw(HDC hdc, const RECT &rect)
{
  if (node_ && node_->props.clearBackground_)
  {
    FillRect(hdc, &rect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
  }
  if (!node_ || !node_->props.model_)
  {
    return;
  }
  const loka::app::RectSurfaceModel model = node_->props.model_->get();
  HBRUSH blackBrush = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  for (short i = 0; i < model.rectCount; ++i)
  {
    RECT spriteRect;
    spriteRect.left = model.rects[i].x;
    spriteRect.top = model.rects[i].y;
    spriteRect.right = static_cast<LONG>(model.rects[i].x + model.rects[i].width);
    spriteRect.bottom = static_cast<LONG>(model.rects[i].y + model.rects[i].height);
    FillRect(hdc, &spriteRect, blackBrush);
  }
}
