#include "Win32ButtonContext.hpp"
#include "../Win32ScenePlatformController.hpp"
#include "app/Button.hpp"
#include "loka/core/State.hpp"
#include "loka/platform/StringUTF8.hpp"

Win32ButtonContext::Win32ButtonContext(HWND parent, int x, int y, int width, int height, loka::app::ButtonNode *node)
    : node_(node),
      hwnd_(NULL),
      textState_(0),
      enabledState_(0)
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
  bindEnabled();
}

Win32ButtonContext::~Win32ButtonContext()
{
  unbindText();
  unbindEnabled();
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
  textState_ = static_cast<loka::core::State<loka::core::String> *>(node_->props.text_);
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

void Win32ButtonContext::bindEnabled()
{
  if (!node_)
    return;
  enabledState_ = static_cast<loka::core::State<bool> *>(node_->props.enabled_);
  if (enabledState_)
  {
    enabledState_->bind(&Win32ButtonContext::EnabledChangedThunk, this, true);
  }
}

void Win32ButtonContext::unbindEnabled()
{
  if (enabledState_)
  {
    enabledState_->unbind(&Win32ButtonContext::EnabledChangedThunk, this);
    enabledState_ = 0;
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
  Win32ScenePlatformController::requestDirtyRect(hwnd_, NULL, TRUE);
}

void Win32ButtonContext::applyEnabled()
{
  if (!hwnd_ || !enabledState_)
  {
    return;
  }
  EnableWindow(hwnd_, enabledState_->get() ? TRUE : FALSE);
  Win32ScenePlatformController::requestDirtyRect(hwnd_, NULL, TRUE);
}

void Win32ButtonContext::TextChangedThunk(void *userData)
{
  Win32ButtonContext *self = static_cast<Win32ButtonContext *>(userData);
  if (self)
  {
    self->applyText();
  }
}

void Win32ButtonContext::EnabledChangedThunk(void *userData)
{
  Win32ButtonContext *self = static_cast<Win32ButtonContext *>(userData);
  if (self)
  {
    self->applyEnabled();
  }
}
