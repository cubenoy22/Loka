#include "Win32ScrollViewContext.hpp"

#include <cassert>

#include "../Win32ScenePlatformController.hpp"

namespace
{
  const wchar_t kScrollViewClassName[] = L"LOKA_SCROLL_VIEW";
}

Win32ScrollViewContext::Win32ScrollViewContext(Win32ScenePlatformController *controller,
                                               HWND parent,
                                               int x,
                                               int y,
                                               int width,
                                               int height,
                                               loka::app::ScrollViewNode *node)
    : Win32RetirableContext(controller),
      node_(node),
      hwnd_(0)
{
  EnsureClassRegistered();
  // WS_EX_CONTROLPARENT: Win32App routes Tab through IsDialogMessage on the
  // root, and the dialog manager only recurses into children of windows
  // carrying this style (the root window sets it too, Win32Window.cpp) —
  // without it every control inside a ScrollView is skipped by keyboard
  // navigation.
  this->hwnd_ = this->createNativeChildWindow(
      WS_EX_CONTROLPARENT,
      kScrollViewClassName,
      L"",
      WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPCHILDREN,
      x,
      y,
      width,
      height,
      parent,
      0,
      GetModuleHandleW(NULL),
      this);
}

Win32ScrollViewContext::~Win32ScrollViewContext()
{
  assert(!this->hwnd_ && "terminal fact delivery must queue the ScrollView HWND before context reclaim");
}

void Win32ScrollViewContext::readLifecycleFactOnAttach()
{
  if (this->node_ && this->node_->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED)
  {
    this->applyAttachedPresentation();
  }
}

void Win32ScrollViewContext::onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                                           loka::app::scene::NodeLifecycleFact next)
{
  (void)previous;
  if (next == loka::app::scene::NODE_FACT_ATTACHED)
  {
    this->applyAttachedPresentation();
  }
  else
  {
    this->applyDetachedPresentation();
    if (next == loka::app::scene::NODE_FACT_RETIRED)
    {
      this->retireWindow(this->hwnd_);
      this->node_ = 0;
    }
  }
}

bool Win32ScrollViewContext::isValid() const
{
  return this->hwnd_ != 0;
}

HWND Win32ScrollViewContext::hwnd() const
{
  return this->hwnd_;
}

void Win32ScrollViewContext::applyAttachedPresentation()
{
  if (this->hwnd_)
  {
    ShowWindow(this->hwnd_, SW_SHOW);
  }
}

void Win32ScrollViewContext::applyDetachedPresentation()
{
  if (this->hwnd_)
  {
    ShowWindow(this->hwnd_, SW_HIDE);
  }
}

void Win32ScrollViewContext::relayout(int x, int y, int width, int height)
{
  if (this->hwnd_)
  {
    this->positionNativeWindow(this->hwnd_, x, y, width, height);
  }
}

int Win32ScrollViewContext::setScrollMetrics(int contentHeight,
                                             int viewportHeight,
                                             int offset)
{
  if (contentHeight < 0)
  {
    contentHeight = 0;
  }
  if (viewportHeight < 0)
  {
    viewportHeight = 0;
  }
  int maximum = contentHeight - viewportHeight;
  if (maximum < 0)
  {
    maximum = 0;
  }
  const int clamped = this->clampOffset(offset, maximum);
  if (!this->hwnd_)
  {
    return clamped;
  }

  SCROLLINFO info;
  ZeroMemory(&info, sizeof(info));
  info.cbSize = sizeof(info);
  // SIF_DISABLENOSCROLL keeps the scrollbar present (disabled) when content
  // fits, so the viewport client width never changes between layout passes
  // and the width read before measurement stays true after it.
  info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
  info.nMin = 0;
  info.nMax = contentHeight > 0 ? contentHeight - 1 : 0;
  info.nPage = viewportHeight > 0 ? static_cast<UINT>(viewportHeight) : 0;
  info.nPos = clamped;
  SetScrollInfo(this->hwnd_, SB_VERT, &info, TRUE);
  return clamped;
}

bool Win32ScrollViewContext::handleVerticalScroll(int command,
                                                  int thumbPosition)
{
  SCROLLINFO info;
  if (!this->readScrollInfo(info))
  {
    return false;
  }

  const int maximum = this->maximumOffset(info);
  int next = this->node_ && this->node_->props.offset_.isValid()
                 ? this->node_->props.offset_.get()
                 : info.nPos;
  next = this->clampOffset(next, maximum);
  int page = info.nPage > static_cast<UINT>(maximum)
                 ? maximum
                 : static_cast<int>(info.nPage);
  switch (command)
  {
  case SB_LINEUP:
    if (next > 0)
    {
      --next;
    }
    this->publishOffset(next);
    return true;
  case SB_LINEDOWN:
    if (next < maximum)
    {
      ++next;
    }
    this->publishOffset(next);
    return true;
  case SB_PAGEUP:
    next = next < page ? 0 : next - page;
    this->publishOffset(next);
    return true;
  case SB_PAGEDOWN:
    next = next > maximum - page ? maximum : next + page;
    this->publishOffset(next);
    return true;
  case SB_THUMBTRACK:
    this->setVisualPosition(this->clampOffset(thumbPosition, maximum));
    return true;
  case SB_THUMBPOSITION:
    next = this->clampOffset(thumbPosition, maximum);
    this->setVisualPosition(next);
    this->publishOffset(next);
    return true;
  case SB_ENDSCROLL:
    this->publishOffset(this->clampOffset(info.nPos, maximum));
    return true;
  default:
    return false;
  }
}

bool Win32ScrollViewContext::readScrollInfo(SCROLLINFO &info) const
{
  if (!this->hwnd_)
  {
    return false;
  }
  ZeroMemory(&info, sizeof(info));
  info.cbSize = sizeof(info);
  info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
  return GetScrollInfo(this->hwnd_, SB_VERT, &info) != FALSE;
}

int Win32ScrollViewContext::maximumOffset(const SCROLLINFO &info) const
{
  int maximum = info.nMax;
  if (info.nPage > 0)
  {
    maximum -= static_cast<int>(info.nPage - 1);
  }
  if (maximum < info.nMin)
  {
    maximum = info.nMin;
  }
  return maximum;
}

int Win32ScrollViewContext::clampOffset(int value, int maximum) const
{
  if (value < 0)
  {
    return 0;
  }
  if (value > maximum)
  {
    return maximum;
  }
  return value;
}

void Win32ScrollViewContext::setVisualPosition(int value)
{
  if (!this->hwnd_)
  {
    return;
  }
  SCROLLINFO info;
  ZeroMemory(&info, sizeof(info));
  info.cbSize = sizeof(info);
  info.fMask = SIF_POS;
  info.nPos = value;
  SetScrollInfo(this->hwnd_, SB_VERT, &info, TRUE);
}

void Win32ScrollViewContext::publishOffset(int value)
{
  if (!this->node_ || !this->node_->props.offset_.isValid() ||
      this->node_->props.offset_.get() == value)
  {
    return;
  }
  this->node_->props.offset_.set(value);
}

void Win32ScrollViewContext::EnsureClassRegistered()
{
  static bool registered = false;
  if (registered)
  {
    return;
  }
  WNDCLASSW wc;
  ZeroMemory(&wc, sizeof(wc));
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = Win32ScrollViewContext::WndProc;
  wc.hInstance = GetModuleHandleW(NULL);
  wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
  wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
  wc.lpszClassName = kScrollViewClassName;
  RegisterClassW(&wc);
  registered = true;
}

LRESULT CALLBACK Win32ScrollViewContext::WndProc(HWND hwnd,
                                                 UINT msg,
                                                 WPARAM wParam,
                                                 LPARAM lParam)
{
  Win32ScrollViewContext *self = static_cast<Win32ScrollViewContext *>(
      reinterpret_cast<void *>(GetWindowLongPtr(hwnd, GWLP_USERDATA)));
  if (msg == WM_NCCREATE)
  {
    CREATESTRUCTW *create = reinterpret_cast<CREATESTRUCTW *>(lParam);
    self = static_cast<Win32ScrollViewContext *>(create->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }

  if (msg == WM_VSCROLL && self &&
      self->handleVerticalScroll(LOWORD(wParam), HIWORD(wParam)))
  {
    return 0;
  }
  if (msg == WM_COMMAND && self && self->controller() &&
      self->controller()->handleCommand(wParam, lParam))
  {
    // Controls parented to the viewport notify it, not the root window;
    // forward through the same controller door Win32Window::WndProc uses so
    // a Button inside a ScrollView still reaches its Loka handler.
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}
