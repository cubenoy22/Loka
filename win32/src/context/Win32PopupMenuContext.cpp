#include "Win32PopupMenuContext.hpp"
#include "../Win32ScenePlatformController.hpp"
#include "app/scene/projection/PlatformNodeHandler.hpp"
#include "platform/StringUTF8.hpp"
#include <string>
#include <tchar.h>

namespace
{
  const int kPopupMenuHeight = 26;
  const int kVerticalSpacing = 12;

  class Win32PopupMenuNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::PopupMenuNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      loka::app::PopupMenuNode *popup = node ? node->asPopupMenuNode() : 0;
      Win32ScenePlatformController *win32 = static_cast<Win32ScenePlatformController *>(controller);
      if (!popup || !win32)
      {
        return 0;
      }
      return win32->contextMapper()->ensurePopupMenuContext(popup, state.x, state.y, state.width, state.height);
    }
  };

  Win32PopupMenuNodeHandler gWin32PopupMenuNodeHandler;
} // namespace

Win32PopupMenuContext::Win32PopupMenuContext(
    HWND parent, int x, int y, int width, int height, loka::app::PopupMenuNode *node)
    : node_(node),
      hwnd_(0),
      selectionState_(0),
      enabledState_(0),
      applyingFromState_(false),
      updatingFromControl_(false),
      baseHeight_(height),
      baseWidth_(width)
{
  hwnd_ = CreateWindowEx(0,
                         TEXT("COMBOBOX"),
                         TEXT(""),
                         WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                         x,
                         y,
                         width,
                         height,
                         parent,
                         0,
                         GetModuleHandle(NULL),
                         NULL);
  if (hwnd_)
  {
    SetWindowLongPtr(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
  }

  applyItems();
  bindSelection();
  bindEnabled();
}

Win32PopupMenuContext::~Win32PopupMenuContext()
{
  unbindSelection();
  unbindEnabled();
  if (hwnd_)
  {
    SetWindowLongPtr(hwnd_, GWLP_USERDATA, 0);
    DestroyWindow(hwnd_);
    hwnd_ = 0;
  }
}

void Win32PopupMenuContext::onNodeAttached()
{
  if (hwnd_)
  {
    ShowWindow(hwnd_, SW_SHOW);
  }
}

void Win32PopupMenuContext::onNodeDetached()
{
  if (hwnd_)
  {
    ShowWindow(hwnd_, SW_HIDE);
  }
}

bool Win32PopupMenuContext::handleCommand(WPARAM, LPARAM)
{
  if (!applyingFromState_)
  {
    syncStateFromControl();
  }
  return true;
}

short Win32PopupMenuContext::layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state)
{
  this->relayout(state.x, state.y, state.width, kPopupMenuHeight);
  state.height = static_cast<short>(kPopupMenuHeight);
  return static_cast<short>(state.y + kPopupMenuHeight + kVerticalSpacing);
}

void Win32PopupMenuContext::relayout(int x, int y, int width, int height)
{
  if (!hwnd_)
  {
    return;
  }
  baseWidth_ = width;
  baseHeight_ = height;
  MoveWindow(hwnd_, x, y, width, height, TRUE);
  applyItems();
  applySelection();
}

void Win32PopupMenuContext::bindSelection()
{
  if (!node_)
  {
    return;
  }
  selectionState_ = static_cast<loka::core::State<int> *>(node_->props.selectedIndex_);
  if (selectionState_)
  {
    selectionState_->bind(&Win32PopupMenuContext::SelectionChangedThunk, this, true);
    applySelection();
  }
}

void Win32PopupMenuContext::unbindSelection()
{
  if (selectionState_)
  {
    selectionState_->unbind(&Win32PopupMenuContext::SelectionChangedThunk, this);
    selectionState_ = 0;
  }
}

void Win32PopupMenuContext::bindEnabled()
{
  if (!node_)
  {
    return;
  }
  enabledState_ = static_cast<loka::core::State<bool> *>(node_->props.enabled_);
  if (enabledState_)
  {
    enabledState_->bind(&Win32PopupMenuContext::EnabledChangedThunk, this, true);
    applyEnabled();
  }
}

void Win32PopupMenuContext::unbindEnabled()
{
  if (enabledState_)
  {
    enabledState_->unbind(&Win32PopupMenuContext::EnabledChangedThunk, this);
    enabledState_ = 0;
  }
}

void Win32PopupMenuContext::applyItems()
{
  if (!hwnd_ || !node_)
  {
    return;
  }
  SendMessage(hwnd_, CB_RESETCONTENT, 0, 0);
  const loka::Vector<loka::core::String> *items = node_->props.items_;
  if (!items)
  {
    return;
  }
  for (std::size_t i = 0; i < items->size(); ++i)
  {
    std::string utf8;
    if (loka::platform::CollectUtf8((*items)[i], utf8))
    {
      std::wstring wide(utf8.begin(), utf8.end());
      SendMessageW(hwnd_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str()));
    }
  }
  int itemHeight = static_cast<int>(SendMessage(hwnd_, CB_GETITEMHEIGHT, 0, 0));
  if (itemHeight <= 0)
  {
    itemHeight = baseHeight_ > 0 ? baseHeight_ : 18;
  }
  int visibleItems = static_cast<int>(items->size());
  if (visibleItems > 8)
  {
    visibleItems = 8;
  }
  int dropHeight = baseHeight_ + itemHeight * visibleItems + 2;
  if (dropHeight < baseHeight_)
  {
    dropHeight = baseHeight_;
  }
  SetWindowPos(hwnd_, 0, 0, 0, baseWidth_, dropHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Win32PopupMenuContext::applySelection()
{
  if (!hwnd_ || !selectionState_)
  {
    return;
  }
  int index = selectionState_->get();
  if (index < 0)
  {
    index = -1;
  }
  applyingFromState_ = true;
  SendMessage(hwnd_, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
  applyingFromState_ = false;
}

void Win32PopupMenuContext::applyEnabled()
{
  if (!hwnd_ || !enabledState_)
  {
    return;
  }
  EnableWindow(hwnd_, enabledState_->get() ? TRUE : FALSE);
}

void Win32PopupMenuContext::syncStateFromControl()
{
  if (!hwnd_ || !selectionState_)
  {
    return;
  }
  loka::core::MutableState<int> *mutableState = dynamic_cast<loka::core::MutableState<int> *>(selectionState_);
  if (!mutableState)
  {
    return;
  }
  updatingFromControl_ = true;
  LRESULT index = SendMessage(hwnd_, CB_GETCURSEL, 0, 0);
  mutableState->set(static_cast<int>(index), true);
  updatingFromControl_ = false;
  if (node_ && node_->props.onChange_)
  {
    node_->props.onChange_->emit();
  }
}

void Win32PopupMenuContext::SelectionChangedThunk(void *userData)
{
  Win32PopupMenuContext *self = static_cast<Win32PopupMenuContext *>(userData);
  if (self)
  {
    self->applySelection();
  }
}

void Win32PopupMenuContext::EnabledChangedThunk(void *userData)
{
  Win32PopupMenuContext *self = static_cast<Win32PopupMenuContext *>(userData);
  if (self)
  {
    self->applyEnabled();
  }
}

void RegisterWin32PopupMenuNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry)
{
  registry.registerHandler(&gWin32PopupMenuNodeHandler);
}
