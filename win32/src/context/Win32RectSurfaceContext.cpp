#include "Win32RectSurfaceContext.hpp"
#include <cassert>
#include "../Win32ScenePlatformController.hpp"
#include "app/RectSurface.hpp"

namespace
{
  const wchar_t kRectSurfaceClassName[] = L"LOKA_RECT_SURFACE";
  const COLORREF kRectSurfaceClearColor = RGB(255, 255, 255);
}

Win32RectSurfaceContext::Win32RectSurfaceContext(Win32ScenePlatformController *controller,
                                                 HWND parent,
                                                 int x,
                                                 int y,
                                                 int width,
                                                 int height,
                                                 loka::app::RectSurfaceNode *node)
    : Win32RetirableContext(controller),
      node_(node),
      hwnd_(0),
      modelState_(0)
{
  EnsureClassRegistered();
  hwnd_ = CreateWindowExW(
      0, kRectSurfaceClassName, L"", WS_CHILD | WS_VISIBLE, x, y, width, height, parent, 0, GetModuleHandleW(NULL), this);
  bindModel();
}

Win32RectSurfaceContext::~Win32RectSurfaceContext()
{
  assert(!hwnd_ && "terminal fact delivery must queue the HWND before context reclaim");
}

void Win32RectSurfaceContext::readLifecycleFactOnAttach()
{
  if (this->node_ && this->node_->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED)
  {
    this->applyAttachedPresentation();
  }
}

void Win32RectSurfaceContext::onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                                            loka::app::scene::NodeLifecycleFact next)
{
  (void)previous;
  if (next == loka::app::scene::NODE_FACT_ATTACHED)
  {
    this->applyAttachedPresentation();
  }
  else
  {
    // DETACHED_RETAINED hides; terminal RETIRED keeps the same policy
    // (hide before the ritual destroys the native pair).
    this->applyDetachedPresentation();
    if (next == loka::app::scene::NODE_FACT_RETIRED)
    {
      this->unbindModel();
      this->retireWindow(this->hwnd_);
      this->node_ = 0;
    }
  }
}

void Win32RectSurfaceContext::applyAttachedPresentation()
{
  if (hwnd_)
  {
    ShowWindow(hwnd_, SW_SHOW);
  }
}

void Win32RectSurfaceContext::applyDetachedPresentation()
{
  if (hwnd_)
  {
    ShowWindow(hwnd_, SW_HIDE);
  }
}

void Win32RectSurfaceContext::relayout(int x, int y, int width, int height)
{
  if (!hwnd_)
  {
    return;
  }
  this->positionNativeWindow(this->hwnd_, x, y, width, height);
  HWND parent = 0;
  RECT rect;
  if (this->queryBoundsInParent(parent, rect))
  {
    Win32ScenePlatformController::redrawDirtySubtreeNow(parent, &rect, TRUE);
  }
}

void Win32RectSurfaceContext::EnsureClassRegistered()
{
  static bool registered = false;
  if (registered)
  {
    return;
  }
  WNDCLASSW wc;
  ZeroMemory(&wc, sizeof(wc));
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = Win32RectSurfaceContext::WndProc;
  wc.hInstance = GetModuleHandleW(NULL);
  wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
  wc.hbrBackground = NULL;
  wc.lpszClassName = kRectSurfaceClassName;
  RegisterClassW(&wc);
  registered = true;
}

LRESULT CALLBACK Win32RectSurfaceContext::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  Win32RectSurfaceContext *self =
      static_cast<Win32RectSurfaceContext *>(reinterpret_cast<void *>(GetWindowLongPtr(hwnd, GWLP_USERDATA)));
  if (msg == WM_NCCREATE)
  {
    CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
    self = static_cast<Win32RectSurfaceContext *>(create->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }

  switch (msg)
  {
  case WM_ERASEBKGND:
    Win32ScenePlatformController::noteNativePaint(hwnd, Win32ScenePlatformController::NATIVE_PAINT_RECT_SURFACE, true);
    return 1;
  case WM_PAINT:
  {
    Win32ScenePlatformController::noteNativePaint(hwnd, Win32ScenePlatformController::NATIVE_PAINT_RECT_SURFACE, false);
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
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void Win32RectSurfaceContext::bindModel()
{
  if (!node_)
  {
    return;
  }
  modelState_ = node_->props.model_;
  if (modelState_)
  {
    modelState_->bind(&Win32RectSurfaceContext::ModelChangedThunk, this, true);
    applyModel();
  }
}

void Win32RectSurfaceContext::unbindModel()
{
  if (modelState_)
  {
    modelState_->unbind(&Win32RectSurfaceContext::ModelChangedThunk, this);
    modelState_ = 0;
  }
}

void Win32RectSurfaceContext::applyModel()
{
  if (!hwnd_)
  {
    return;
  }
  HWND parent = 0;
  RECT rect;
  if (this->queryBoundsInParent(parent, rect))
  {
    Win32ScenePlatformController::requestDirtySubtree(parent, &rect, TRUE);
    return;
  }
  Win32ScenePlatformController::requestDirtyRect(hwnd_, NULL, TRUE);
}

bool Win32RectSurfaceContext::queryBoundsInParent(HWND &parent, RECT &rect) const
{
  parent = this->hwnd_ ? GetParent(this->hwnd_) : 0;
  if (!parent || !GetWindowRect(this->hwnd_, &rect))
  {
    return false;
  }
  MapWindowPoints(NULL, parent, reinterpret_cast<POINT *>(&rect), 2);
  return true;
}

void Win32RectSurfaceContext::ModelChangedThunk(void *userData)
{
  Win32RectSurfaceContext *self = static_cast<Win32RectSurfaceContext *>(userData);
  if (self)
  {
    self->applyModel();
  }
}

void Win32RectSurfaceContext::draw(HDC hdc, const RECT &rect)
{
  if (node_ && node_->props.clearBackground_)
  {
    HBRUSH backgroundBrush = CreateSolidBrush(kRectSurfaceClearColor);
    if (backgroundBrush)
    {
      FillRect(hdc, &rect, backgroundBrush);
      DeleteObject(backgroundBrush);
    }
  }
  if (!node_ || !modelState_)
  {
    return;
  }
  const loka::app::RectSurfaceModel model = modelState_->get();
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
