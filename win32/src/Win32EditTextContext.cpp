#include "Win32EditTextContext.hpp"
#include <vector>
#include "app2/EditText.hpp"
#include "core/State.hpp"

Win32EditTextContext::Win32EditTextContext(HWND parent, int x, int y, int width, int height, declara::app::EditTextNode *node)
    : node_(node), hwnd_(NULL), textState_(0), applyingFromState_(false), updatingFromControl_(false)
{
  DWORD style = WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL;
  hwnd_ = CreateWindowExA(
      WS_EX_CLIENTEDGE,
      "EDIT",
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
  bindText();
}

Win32EditTextContext::~Win32EditTextContext()
{
  unbindText();
  if (hwnd_)
  {
    DestroyWindow(hwnd_);
    hwnd_ = NULL;
  }
}

bool Win32EditTextContext::handleCommand(WPARAM wParam, LPARAM)
{
  WORD code = HIWORD(wParam);
  if (code == EN_CHANGE)
  {
    if (!applyingFromState_)
    {
      syncStateFromControl();
    }
    return true;
  }
  return false;
}

void Win32EditTextContext::bindText()
{
  if (!node_)
  {
    return;
  }
  textState_ = static_cast<State<std::string> *>(node_->props.text);
  if (textState_)
  {
    textState_->bind(&Win32EditTextContext::TextChangedThunk, this, true);
  }
}

void Win32EditTextContext::unbindText()
{
  if (textState_)
  {
    textState_->unbind(&Win32EditTextContext::TextChangedThunk, this);
    textState_ = 0;
  }
}

void Win32EditTextContext::applyText()
{
  if (!hwnd_ || !textState_)
  {
    return;
  }
  if (updatingFromControl_)
  {
    return;
  }
  int currentLen = GetWindowTextLengthA(hwnd_);
  std::vector<char> buffer(currentLen + 1);
  if (currentLen >= 0)
  {
    GetWindowTextA(hwnd_, &buffer[0], currentLen + 1);
  }
  else
  {
    buffer.assign(1, '\0');
  }
  std::string currentText(&buffer[0]);
  const std::string &desired = textState_->get();
  if (currentText == desired)
  {
    return;
  }
  DWORD selStart = 0;
  DWORD selEnd = 0;
  SendMessageA(hwnd_, EM_GETSEL, reinterpret_cast<WPARAM>(&selStart), reinterpret_cast<LPARAM>(&selEnd));
  applyingFromState_ = true;
  SetWindowTextA(hwnd_, desired.c_str());
  SendMessageA(hwnd_, EM_SETSEL, selStart, selEnd);
  applyingFromState_ = false;
}

void Win32EditTextContext::syncStateFromControl()
{
  if (!textState_ || !hwnd_)
  {
    return;
  }
  MutableState<std::string> *mutableState = dynamic_cast<MutableState<std::string> *>(textState_);
  if (!mutableState)
  {
    return;
  }
  updatingFromControl_ = true;
  int length = GetWindowTextLengthA(hwnd_);
  if (length < 0)
  {
    length = 0;
  }
  std::vector<char> buffer(length + 1);
  GetWindowTextA(hwnd_, &buffer[0], length + 1);
  mutableState->set(std::string(&buffer[0]), true);
  updatingFromControl_ = false;
}

void Win32EditTextContext::TextChangedThunk(void *userData)
{
  Win32EditTextContext *self = static_cast<Win32EditTextContext *>(userData);
  if (self)
  {
    self->applyText();
  }
}
