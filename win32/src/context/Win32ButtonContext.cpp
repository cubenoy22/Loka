#include "Win32ButtonContext.hpp"
#include "app/Button.hpp"
#include "core/State.hpp"
#include "loka/platform/StringUTF8.hpp"

Win32ButtonContext::Win32ButtonContext(HWND parent, int x, int y, int width, int height, loka::app::ButtonNode *node)
    : node_(node),
      hwnd_(NULL),
      textState_(0)
{
  DWORD style = WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON;
  hwnd_ = CreateWindowExA(
      0,
      "BUTTON",
      "",
      style,
      x,
      y,
      width,
      height,
      parent,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(1000)),
      GetModuleHandle(NULL),
      NULL);
  bindText();
}

Win32ButtonContext::~Win32ButtonContext()
{
  unbindText();
  if (hwnd_)
  {
    DestroyWindow(hwnd_);
    hwnd_ = NULL;
  }
}

bool Win32ButtonContext::handleCommand(WPARAM, LPARAM)
{
  if (node_ && node_->props.onClick_)
  {
    node_->props.onClick_->emit();
    return true;
  }
  return false;
}

void Win32ButtonContext::bindText()
{
  if (!node_)
    return;
  textState_ = static_cast<State<loka::core::String> *>(node_->props.text_);
  if (textState_)
  {
    textState_->bind(&Win32ButtonContext::TextChangedThunk, this, true);
  }
}

void Win32ButtonContext::unbindText()
{
  if (textState_)
  {
    textState_->unbind(&Win32ButtonContext::TextChangedThunk, this);
    textState_ = 0;
  }
}

void Win32ButtonContext::applyText()
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
}

void Win32ButtonContext::TextChangedThunk(void *userData)
{
  Win32ButtonContext *self = static_cast<Win32ButtonContext *>(userData);
  if (self)
  {
    self->applyText();
  }
}
