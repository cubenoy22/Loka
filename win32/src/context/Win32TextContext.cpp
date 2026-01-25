#include "Win32TextContext.hpp"
#include "app/Text.hpp"
#include "core/State.hpp"
#include "loka/platform/StringUTF8.hpp"

Win32TextContext::Win32TextContext(HWND parent, int x, int y, int width, int height, loka::app::TextNode *node)
    : node_(node), hwnd_(NULL), textState_(0)
{
  DWORD style = WS_VISIBLE | WS_CHILD | SS_LEFT;
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
}

void Win32TextContext::TextChangedThunk(void *userData)
{
  Win32TextContext *self = static_cast<Win32TextContext *>(userData);
  if (self)
  {
    self->applyText();
  }
}
