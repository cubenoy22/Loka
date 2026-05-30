#include "Win32EditTextContext.hpp"
#include "../Win32ScenePlatformController.hpp"
#include "app/scene/projection/PlatformNodeHandler.hpp"
#include <vector>
#include "app/nodes/controls/EditText.hpp"
#include "core/State.hpp"
#include "platform/StringUTF8.hpp"

namespace
{
  const int kEditTextHeight = 24;
  const int kVerticalSpacing = 12;

  class Win32EditTextNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::EditTextNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      loka::app::EditTextNode *edit = node ? node->asEditTextNode() : 0;
      Win32ScenePlatformController *win32 = static_cast<Win32ScenePlatformController *>(controller);
      if (!edit || !win32)
      {
        return 0;
      }
      return win32->contextMapper()->ensureEditTextContext(edit, state.x, state.y, state.width, state.height);
    }
  };

  Win32EditTextNodeHandler gWin32EditTextNodeHandler;
} // namespace

Win32EditTextContext::Win32EditTextContext(
    HWND parent, int x, int y, int width, int height, loka::app::EditTextNode *node)
    : node_(node),
      hwnd_(NULL),
      textState_(0),
      applyingFromState_(false),
      updatingFromControl_(false)
{
  DWORD style = WS_TABSTOP | WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL;
  hwnd_ = CreateWindowExA(
      WS_EX_CLIENTEDGE, "EDIT", "", style, x, y, width, height, parent, NULL, GetModuleHandle(NULL), NULL);
  if (hwnd_)
  {
    SetWindowLongPtr(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
  }
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

void Win32EditTextContext::onNodeAttached()
{
  if (hwnd_)
  {
    ShowWindow(hwnd_, SW_SHOW);
  }
}

void Win32EditTextContext::onNodeDetached()
{
  if (hwnd_)
  {
    ShowWindow(hwnd_, SW_HIDE);
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

short Win32EditTextContext::layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state)
{
  this->relayout(state.x, state.y, state.width, kEditTextHeight);
  state.height = static_cast<short>(kEditTextHeight);
  return static_cast<short>(state.y + kEditTextHeight + kVerticalSpacing);
}

void Win32EditTextContext::relayout(int x, int y, int width, int height)
{
  if (!hwnd_)
  {
    return;
  }
  MoveWindow(hwnd_, x, y, width, height, TRUE);
}

void Win32EditTextContext::bindText()
{
  if (!node_)
  {
    return;
  }
  textState_ = static_cast<loka::core::State<loka::core::String> *>(node_->props.text_);
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
  std::string desired;
  if (!loka::platform::CollectUtf8(textState_->get(), desired))
  {
    desired.clear();
  }
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
  loka::core::MutableState<loka::core::String> *mutableState =
      dynamic_cast<loka::core::MutableState<loka::core::String> *>(textState_);
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
  mutableState->set(loka::core::String(std::string(&buffer[0])), true);
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

void RegisterWin32EditTextNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  registry.registerHandler(&gWin32EditTextNodeHandler);
}
