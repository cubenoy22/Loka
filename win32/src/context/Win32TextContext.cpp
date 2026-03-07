#include "Win32TextContext.hpp"
#include "app/Text.hpp"
#include "loka/core/State.hpp"
#include "loka/platform/StringUTF8.hpp"

Win32TextContext::Win32TextContext(HWND parent, int x, int y, int width, int height, loka::app::TextNode *node)
    : node_(node), hwnd_(NULL), textState_(0)
{
  DWORD style = WS_VISIBLE | WS_CHILD | SS_LEFT;
  if (node_ && node_->props.hasAttr_)
  {
    const loka::app::TextAttr &attr = node_->props.attr_;
    const bool wrapWord = attr.hasWrapValue_ && attr.wrapValue_ == loka::app::TEXT_WRAP_WORD;
    const bool truncEllipsis = attr.hasTruncationValue_ &&
                               attr.truncationValue_ == loka::app::TEXT_TRUNCATION_ELLIPSIS;
    if (!wrapWord)
    {
      style |= SS_LEFTNOWORDWRAP;
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
      InvalidateRect(parent, &rc, TRUE);
    }
  }
  InvalidateRect(hwnd_, NULL, TRUE);
}

void Win32TextContext::TextChangedThunk(void *userData)
{
  Win32TextContext *self = static_cast<Win32TextContext *>(userData);
  if (self)
  {
    self->applyText();
  }
}
