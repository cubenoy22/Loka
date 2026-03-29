#include "Win32CellContext.hpp"
#include "../Win32ScenePlatformController.hpp"
#include "app/scene/PlatformNodeHandler.hpp"
#include "app/Cell.hpp"
#include "loka/core/State.hpp"
#include "loka/platform/StringUTF8.hpp"

namespace
{
  const char *kCellClassName = "LOKA_CELL";
  const int kDefaultCellHeight = 20;
  const int kVerticalSpacing = 12;

  class Win32CellNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::CellNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      loka::app::CellNode *cell = node ? node->asCellNode() : 0;
      Win32ScenePlatformController *win32 = static_cast<Win32ScenePlatformController *>(controller);
      if (!cell || !win32)
      {
        return 0;
      }
      return win32->contextMapper()->ensureCellContext(cell,
                                                       state.x,
                                                       state.y,
                                                       state.width,
                                                       state.height);
    }
  };

  Win32CellNodeHandler gWin32CellNodeHandler;
}

Win32CellContext::Win32CellContext(HWND parent, int x, int y, int width, int height, loka::app::CellNode *node)
    : node_(node),
      hwnd_(0),
      textState_(0),
      text_()
{
  EnsureClassRegistered();
  hwnd_ = CreateWindowExA(
      0,
      kCellClassName,
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
  bindText();
}

Win32CellContext::~Win32CellContext()
{
  unbindText();
  if (hwnd_)
  {
    DestroyWindow(hwnd_);
    hwnd_ = 0;
  }
}

short Win32CellContext::layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state)
{
  const short requestedHeight = state.height;
  const int cellHeight = requestedHeight > 0 ? requestedHeight : kDefaultCellHeight;
  this->relayout(state.x, state.y, state.width, cellHeight);
  state.height = static_cast<short>(cellHeight);
  short result = static_cast<short>(state.y + cellHeight);
  if (requestedHeight <= 0)
  {
    result = static_cast<short>(result + kVerticalSpacing);
  }
  return result;
}

void Win32CellContext::relayout(int x, int y, int width, int height)
{
  if (!hwnd_)
  {
    return;
  }
  MoveWindow(hwnd_, x, y, width, height, TRUE);
  Win32ScenePlatformController::redrawDirtySubtreeNow(hwnd_, NULL, TRUE);
}

void Win32CellContext::EnsureClassRegistered()
{
  static bool registered = false;
  if (registered)
  {
    return;
  }
  WNDCLASSA wc;
  ZeroMemory(&wc, sizeof(wc));
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = Win32CellContext::WndProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = NULL;
  wc.lpszClassName = kCellClassName;
  RegisterClassA(&wc);
  registered = true;
}

LRESULT CALLBACK Win32CellContext::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  Win32CellContext *self = static_cast<Win32CellContext *>(reinterpret_cast<void *>(GetWindowLongPtr(hwnd, GWLP_USERDATA)));
  if (msg == WM_NCCREATE)
  {
    CREATESTRUCT *cs = reinterpret_cast<CREATESTRUCT *>(lParam);
    self = static_cast<Win32CellContext *>(cs->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }
  switch (msg)
  {
  case WM_ERASEBKGND:
    Win32ScenePlatformController::noteNativePaint(hwnd, Win32ScenePlatformController::NATIVE_PAINT_CELL, true);
    return 1;
  case WM_PAINT:
  {
    Win32ScenePlatformController::noteNativePaint(hwnd, Win32ScenePlatformController::NATIVE_PAINT_CELL, false);
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    if (self)
    {
      RECT rect;
      GetClientRect(hwnd, &rect);
      self->drawCell(hdc, rect);
    }
    EndPaint(hwnd, &ps);
    return 0;
  }
  case WM_LBUTTONUP:
    if (self && self->node_ && self->node_->props.onClick_)
    {
      self->node_->props.onClick_->emit();
    }
    return 0;
  default:
    break;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

void Win32CellContext::bindText()
{
  if (!node_)
  {
    return;
  }
  textState_ = static_cast<loka::core::State<loka::core::String> *>(node_->props.text_);
  if (textState_)
  {
    textState_->bind(&Win32CellContext::TextChangedThunk, this, true);
  }
}

void Win32CellContext::unbindText()
{
  if (textState_)
  {
    textState_->unbind(&Win32CellContext::TextChangedThunk, this);
    textState_ = 0;
  }
}

void Win32CellContext::applyText()
{
  if (!textState_)
  {
    return;
  }
  std::string utf8;
  if (loka::platform::CollectUtf8(textState_->get(), utf8))
  {
    text_ = utf8;
  }
  else
  {
    text_.clear();
  }
  if (hwnd_)
  {
    Win32ScenePlatformController::redrawDirtySubtreeNow(hwnd_, NULL, TRUE);
  }
}

void Win32CellContext::drawCell(HDC hdc, const RECT &rect)
{
  HBRUSH fill = CreateSolidBrush(RGB(235, 235, 235));
  FillRect(hdc, &rect, fill);
  DeleteObject(fill);
  FrameRect(hdc, &rect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
  if (!text_.empty())
  {
    SetBkMode(hdc, TRANSPARENT);
    RECT textRect = rect;
    DrawTextA(hdc, text_.c_str(), static_cast<int>(text_.size()),
              &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
  }
}

void Win32CellContext::TextChangedThunk(void *userData)
{
  Win32CellContext *self = static_cast<Win32CellContext *>(userData);
  if (self)
  {
    self->applyText();
  }
}

void RegisterWin32CellNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  registry.registerHandler(&gWin32CellNodeHandler);
}
