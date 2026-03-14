#include "Win32TextContext.hpp"
#include "../Win32ScenePlatformController.hpp"
#include "app/Text.hpp"
#include "loka/core/State.hpp"
#include "loka/platform/StringUTF8.hpp"

Win32TextContext::Win32TextContext(HWND parent, int x, int y, int width, int height, loka::app::TextNode *node)
    : node_(node), hwnd_(NULL), textState_(0), didInitialApply_(false)
{
  DWORD style = WS_VISIBLE | WS_CHILD | SS_LEFT;
  if (node_ && node_->props.hasAttr_)
  {
    const loka::app::TextAttr &attr = node_->props.attr_;
    const bool wrapEnabled = attr.hasWrapValue_ &&
                             (attr.wrapValue_ == loka::app::TEXT_WRAP_WORD ||
                              attr.wrapValue_ == loka::app::TEXT_WRAP_CHAR);
    const bool truncEllipsis = attr.hasTruncationValue_ &&
                               attr.truncationValue_ == loka::app::TEXT_TRUNCATION_ELLIPSIS;
    if (!wrapEnabled)
    {
      style |= SS_LEFTNOWORDWRAP;
    }
    else
    {
      style |= SS_EDITCONTROL;
    }
    if (truncEllipsis)
    {
      style |= SS_ENDELLIPSIS;
    }
  }
  hwnd_ = CreateWindowExA(
      0,
      "STATIC",
      "",
      style,
      x,
      y,
      width,
      height,
      parent,
      NULL,
      GetModuleHandle(NULL),
      NULL);
  if (hwnd_)
  {
    HDC hdc = GetDC(hwnd_);
    if (hdc)
    {
      SetBkMode(hdc, TRANSPARENT);
      ReleaseDC(hwnd_, hdc);
    }
  }
  bindText();
}

Win32TextContext::~Win32TextContext()
{
  unbindText();
  if (hwnd_)
  {
    DestroyWindow(hwnd_);
    hwnd_ = NULL;
  }
}

void Win32TextContext::relayout(int x, int y, int width, int height)
{
  if (!hwnd_)
  {
    return;
  }
  MoveWindow(hwnd_, x, y, width, height, TRUE);
  HWND parent = GetParent(hwnd_);
  if (parent)
  {
    RECT rc;
    if (GetWindowRect(hwnd_, &rc))
    {
      MapWindowPoints(NULL, parent, reinterpret_cast<POINT *>(&rc), 2);
      Win32ScenePlatformController::redrawDirtySubtreeNow(parent, &rc, TRUE);
    }
  }
}

void Win32TextContext::bindText()
{
  if (!node_)
  {
    return;
  }
  textState_ = static_cast<loka::core::State<loka::core::String> *>(node_->props.text_);
  if (textState_)
  {
    textState_->bind(&Win32TextContext::TextChangedThunk, this, true);
  }
}

void Win32TextContext::unbindText()
{
  if (textState_)
  {
    textState_->unbind(&Win32TextContext::TextChangedThunk, this);
    textState_ = 0;
  }
}

void Win32TextContext::applyText()
{
  if (!hwnd_ || !textState_)
  {
    return;
  }
  std::string utf8;
  if (loka::platform::CollectUtf8(textState_->get(), utf8))
  {
    SetWindowTextA(hwnd_, utf8.c_str());
  }
  else
  {
    SetWindowTextA(hwnd_, "");
  }
  HWND parent = GetParent(hwnd_);
  if (parent)
  {
    RECT rc;
    if (GetWindowRect(hwnd_, &rc))
    {
      MapWindowPoints(NULL, parent, reinterpret_cast<POINT *>(&rc), 2);
      Win32ScenePlatformController::redrawDirtySubtreeNow(parent, &rc, TRUE);
    }
  }
  requestRelayoutIfNeeded();
  if (!didInitialApply_)
  {
    didInitialApply_ = true;
  }
}

void Win32TextContext::requestRelayoutIfNeeded()
{
  if (!didInitialApply_ || !node_ || !node_->props.hasAttr_ || !node_->props.attr_.hasWrapValue_)
  {
    return;
  }
  if (node_->props.attr_.wrapValue_ == loka::app::TEXT_WRAP_NONE)
  {
    return;
  }
  HWND parent = GetParent(hwnd_);
  if (!parent)
  {
    return;
  }
  RECT rc;
  if (!GetClientRect(parent, &rc))
  {
    return;
  }
  const int width = rc.right - rc.left;
  const int height = rc.bottom - rc.top;
  PostMessage(parent, WM_SIZE, static_cast<WPARAM>(SIZE_RESTORED),
              static_cast<LPARAM>(MAKELPARAM(width, height)));
}

void Win32TextContext::TextChangedThunk(void *userData)
{
  Win32TextContext *self = static_cast<Win32TextContext *>(userData);
  if (self)
  {
    self->applyText();
  }
}
