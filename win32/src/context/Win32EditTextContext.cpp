#include "Win32EditTextContext.hpp"
#include "../Win32ScenePlatformController.hpp"
#include "app/layout/FallbackControlMetrics.hpp"
#include "app/scene/projection/RetainedNodeHandler.hpp"
#include <vector>
#include "app/nodes/controls/EditText.hpp"
#include "core/State.hpp"
#include "Win32EditTextBridge.hpp"

namespace
{
  class Win32EditTextNodeHandler
      : public loka::app::scene::RetainedNodeHandler<Win32EditTextNodeHandler,
                                                     loka::app::EditTextNode,
                                                     Win32EditTextContext>
  {
  public:
    static loka::app::EditTextNode *cast(loka::app::scene::Node *node)
    {
      return node ? node->asEditTextNode() : 0;
    }

    static Win32EditTextContext *create(loka::app::EditTextNode *edit,
                                        loka::app::scene::IPlatformController *controller,
                                        const loka::app::scene::LayoutState &state)
    {
      Win32ScenePlatformController *win32 = static_cast<Win32ScenePlatformController *>(controller);
      return new Win32EditTextContext(win32->rootHwnd(), state.x, state.y, state.width, state.height, edit);
    }

    static void refresh(Win32EditTextContext *ctx, const loka::app::scene::LayoutState &state)
    {
      ctx->relayout(state.x, state.y, state.width, state.height);
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
  hwnd_ = loka::win32::CreateEditTextControl(parent, x, y, width, height);
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
    SetWindowLongPtr(hwnd_, GWLP_USERDATA, 0);
    DestroyWindow(hwnd_);
    hwnd_ = NULL;
  }
}

void Win32EditTextContext::readLifecycleFactOnAttach()
{
  if (this->node_ && this->node_->lifecycleFact() == loka::app::scene::NODE_FACT_ATTACHED)
  {
    this->applyAttachedPresentation();
  }
}

void Win32EditTextContext::onFactChanged(loka::app::scene::NodeLifecycleFact previous,
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
  }
}

void Win32EditTextContext::applyAttachedPresentation()
{
  if (hwnd_)
  {
    ShowWindow(hwnd_, SW_SHOW);
  }
}

void Win32EditTextContext::applyDetachedPresentation()
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
  this->relayout(state.x, state.y, state.width, loka::app::layout::FallbackControlMetrics::kEditTextHeight);
  state.height = static_cast<short>(loka::app::layout::FallbackControlMetrics::kEditTextHeight);
  return static_cast<short>(state.y + loka::app::layout::FallbackControlMetrics::kEditTextHeight
                            + loka::app::layout::FallbackControlMetrics::kVerticalSpacing);
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
  std::wstring currentText;
  loka::win32::ReadEditTextWide(hwnd_, currentText);
  std::wstring desired;
  if (!loka::win32::MaterializeWideString(textState_->get(), desired))
  {
    desired.clear();
  }
  if (currentText == desired)
  {
    return;
  }
  DWORD selStart = 0;
  DWORD selEnd = 0;
  SendMessageW(hwnd_, EM_GETSEL, reinterpret_cast<WPARAM>(&selStart), reinterpret_cast<LPARAM>(&selEnd));
  applyingFromState_ = true;
  loka::win32::WriteEditTextString(hwnd_, textState_->get());
  SendMessageW(hwnd_, EM_SETSEL, selStart, selEnd);
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
  mutableState->set(loka::win32::ReadEditTextString(hwnd_), true);
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
