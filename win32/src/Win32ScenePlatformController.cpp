#include "Win32ScenePlatformController.hpp"
#include <windows.h>
#include <vector>
#include "app2/Box.hpp"
#include "app2/Button.hpp"
#include "app2/EditText.hpp"
#include "app2/RowColumn.hpp"
#include "app2/Text.hpp"
#include "core2/scene/Node.hpp"

namespace
{
  const int kButtonHeight = 32;
  const int kEditTextHeight = 24;
  const int kTextHeight = 20;
  const int kVerticalSpacing = 12;
  const int kHorizontalSpacing = 12;
}

Win32ScenePlatformController::Win32ScenePlatformController(HWND rootHwnd)
    : rootHwnd_(rootHwnd), rootNode_(0), clientWidth_(0), clientHeight_(0)
{
}

Win32ScenePlatformController::~Win32ScenePlatformController()
{
  clearContexts();
}

void Win32ScenePlatformController::materialize(declara::core::scene::Node *rootNode)
{
  rootNode_ = rootNode;
  if (!rootHwnd_ || !rootNode_)
  {
    return;
  }

  RECT rc;
  if (GetClientRect(rootHwnd_, &rc))
  {
    clientWidth_ = rc.right - rc.left;
    clientHeight_ = rc.bottom - rc.top;
  }
  performLayout(clientWidth_, clientHeight_);
}

void Win32ScenePlatformController::synchronize()
{
  // Solid-mode（固定ツリー）では即時反映済みのため、現状何もしない。
}

void Win32ScenePlatformController::destroy()
{
  clearContexts();
  rootNode_ = 0;
  clientWidth_ = 0;
  clientHeight_ = 0;
}

bool Win32ScenePlatformController::handleCommand(WPARAM wParam, LPARAM lParam)
{
  HWND target = reinterpret_cast<HWND>(lParam);
  WORD code = HIWORD(wParam);
  if (code == BN_CLICKED)
  {
    std::map<HWND, ButtonContext *>::iterator it = buttonMap_.find(target);
    if (it == buttonMap_.end())
    {
      return false;
    }
    return it->second->handleCommand(wParam, lParam);
  }
  if (code == EN_CHANGE)
  {
    std::map<HWND, EditTextContext *>::iterator itEdit = editMap_.find(target);
    if (itEdit == editMap_.end())
    {
      return false;
    }
    return itEdit->second->handleCommand(wParam, lParam);
  }
  return false;
}

void Win32ScenePlatformController::relayout(int clientWidth, int clientHeight)
{
  if (!rootNode_)
  {
    return;
  }
  if (clientWidth <= 0 || clientHeight <= 0)
  {
    RECT rc;
    if (rootHwnd_ && GetClientRect(rootHwnd_, &rc))
    {
      clientWidth = rc.right - rc.left;
      clientHeight = rc.bottom - rc.top;
    }
  }
  clientWidth_ = clientWidth;
  clientHeight_ = clientHeight;
  performLayout(clientWidth_, clientHeight_);
}

void Win32ScenePlatformController::performLayout(int clientWidth, int clientHeight)
{
  clearContexts();
  if (!rootNode_ || !rootHwnd_)
  {
    return;
  }
  LayoutState state;
  state.x = 20;
  state.y = 20;
  state.width = measureClientWidth(clientWidth) - 40;
  if (state.width < 0)
  {
    state.width = 0;
  }
  state.height = clientHeight > 0 ? clientHeight - 40 : 0;
  layoutNode(rootNode_, state);
}

int Win32ScenePlatformController::layoutNode(declara::core::scene::Node *node, const LayoutState &state)
{
  if (!node)
  {
    return state.y;
  }

  if (declara::app::RowNode *row = dynamic_cast<declara::app::RowNode *>(node))
  {
    const std::vector<declara::core::scene::Node *> &children = row->getChildren();
    if (children.empty())
    {
      return state.y;
    }
    LayoutState childState = state;
    const size_t childCount = children.size();
    const int childCountInt = static_cast<int>(childCount);
    const int gap = kHorizontalSpacing;
    const int spacingTotal = gap * (childCountInt > 0 ? childCountInt - 1 : 0);
    int availableWidth = state.width - spacingTotal;
    if (availableWidth < 0)
    {
      availableWidth = 0;
    }
    const int baseWidth = childCountInt > 0 ? availableWidth / childCountInt : 0;
    int remainder = childCountInt > 0 ? availableWidth - baseWidth * childCountInt : 0;
    int currentX = state.x;
    int maxY = state.y;
    for (size_t i = 0; i < childCount; ++i)
    {
      int childWidth = baseWidth;
      if (remainder > 0)
      {
        childWidth += 1;
        remainder -= 1;
      }
      childState.x = currentX;
      childState.y = state.y;
      childState.width = childWidth;
      int childY = layoutNode(children[i], childState);
      if (childY > maxY)
      {
        maxY = childY;
      }
      currentX += childWidth + gap;
    }
    return maxY;
  }

  if (declara::core::scene::INestable *nestable = dynamic_cast<declara::core::scene::INestable *>(node))
  {
    LayoutState childState = state;
    const std::vector<declara::core::scene::Node *> &children = nestable->getChildren();
    for (size_t i = 0; i < children.size(); ++i)
    {
      childState.y = layoutNode(children[i], childState);
    }
    return childState.y;
  }

  if (declara::app::ButtonNode *button = dynamic_cast<declara::app::ButtonNode *>(node))
  {
    ButtonContext *ctx = new ButtonContext(rootHwnd_, state.x, state.y, state.width, button);
    contexts_.push_back(ctx);
    buttonMap_[ctx->hwnd()] = ctx;

    LayoutState nextState = state;
    nextState.y = state.y + kButtonHeight + kVerticalSpacing;
    return nextState.y;
  }

  if (declara::app::EditTextNode *edit = dynamic_cast<declara::app::EditTextNode *>(node))
  {
    EditTextContext *ctx = new EditTextContext(rootHwnd_, state.x, state.y, state.width, edit);
    contexts_.push_back(ctx);
    editMap_[ctx->hwnd()] = ctx;

    LayoutState nextState = state;
    nextState.y = state.y + kEditTextHeight + kVerticalSpacing;
    return nextState.y;
  }

  if (declara::app::TextNode *text = dynamic_cast<declara::app::TextNode *>(node))
  {
    TextContext *ctx = new TextContext(rootHwnd_, state.x, state.y, state.width, text);
    contexts_.push_back(ctx);

    LayoutState nextState = state;
    nextState.y = state.y + kTextHeight + kVerticalSpacing;
    return nextState.y;
  }

  return state.y;
}

void Win32ScenePlatformController::clearContexts()
{
  for (size_t i = 0; i < contexts_.size(); ++i)
  {
    if (contexts_[i])
    {
      contexts_[i]->destroy();
      delete contexts_[i];
    }
  }
  contexts_.clear();
  buttonMap_.clear();
  editMap_.clear();
}

int Win32ScenePlatformController::measureClientWidth(int requestedWidth) const
{
  if (requestedWidth > 0)
  {
    return requestedWidth;
  }
  RECT rc;
  if (rootHwnd_ && GetClientRect(rootHwnd_, &rc))
  {
    return rc.right - rc.left;
  }
  return 260;
}

Win32ScenePlatformController::ButtonContext::ButtonContext(HWND parent, int x, int y, int width, declara::app::ButtonNode *node)
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
      kButtonHeight,
      parent,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(1000)),
      GetModuleHandle(NULL),
      NULL);
  bindText();
}

Win32ScenePlatformController::ButtonContext::~ButtonContext()
{
  unbindText();
}

void Win32ScenePlatformController::ButtonContext::destroy()
{
  unbindText();
  if (hwnd_)
  {
    DestroyWindow(hwnd_);
    hwnd_ = NULL;
  }
}

bool Win32ScenePlatformController::ButtonContext::handleCommand(WPARAM, LPARAM)
{
  if (node_ && node_->props.onClick)
  {
    node_->props.onClick->emit();
    return true;
  }
  return false;
}

void Win32ScenePlatformController::ButtonContext::bindText()
{
  if (!node_)
    return;
  textState_ = node_->props.text;
  if (textState_)
  {
    textState_->bind(&ButtonContext::TextChangedThunk, this, true);
  }
}

void Win32ScenePlatformController::ButtonContext::unbindText()
{
  if (textState_)
  {
    textState_->unbind(&ButtonContext::TextChangedThunk, this);
    textState_ = 0;
  }
}

void Win32ScenePlatformController::ButtonContext::applyText()
{
  if (!hwnd_ || !textState_)
  {
    return;
  }
  SetWindowTextA(hwnd_, textState_->get().c_str());
}

void Win32ScenePlatformController::ButtonContext::TextChangedThunk(void *userData)
{
  ButtonContext *self = static_cast<ButtonContext *>(userData);
  if (self)
  {
    self->applyText();
  }
}

Win32ScenePlatformController::TextContext::TextContext(HWND parent, int x, int y, int width, declara::app::TextNode *node)
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
      kTextHeight,
      parent,
      NULL,
      GetModuleHandle(NULL),
      NULL);
  bindText();
}

Win32ScenePlatformController::TextContext::~TextContext()
{
  unbindText();
}

void Win32ScenePlatformController::TextContext::destroy()
{
  unbindText();
  if (hwnd_)
  {
    DestroyWindow(hwnd_);
    hwnd_ = NULL;
  }
}

void Win32ScenePlatformController::TextContext::bindText()
{
  if (!node_)
  {
    return;
  }
  textState_ = node_->props.text;
  if (textState_)
  {
    textState_->bind(&TextContext::TextChangedThunk, this, true);
  }
}

void Win32ScenePlatformController::TextContext::unbindText()
{
  if (textState_)
  {
    textState_->unbind(&TextContext::TextChangedThunk, this);
    textState_ = 0;
  }
}

void Win32ScenePlatformController::TextContext::applyText()
{
  if (!hwnd_ || !textState_)
  {
    return;
  }
  SetWindowTextA(hwnd_, textState_->get().c_str());
}

void Win32ScenePlatformController::TextContext::TextChangedThunk(void *userData)
{
  TextContext *self = static_cast<TextContext *>(userData);
  if (self)
  {
    self->applyText();
  }
}

Win32ScenePlatformController::EditTextContext::EditTextContext(HWND parent, int x, int y, int width, declara::app::EditTextNode *node)
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
      kEditTextHeight,
      parent,
      NULL,
      GetModuleHandle(NULL),
      NULL);
  bindText();
}

Win32ScenePlatformController::EditTextContext::~EditTextContext()
{
  unbindText();
}

void Win32ScenePlatformController::EditTextContext::destroy()
{
  unbindText();
  if (hwnd_)
  {
    DestroyWindow(hwnd_);
    hwnd_ = NULL;
  }
}

bool Win32ScenePlatformController::EditTextContext::handleCommand(WPARAM wParam, LPARAM)
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

void Win32ScenePlatformController::EditTextContext::bindText()
{
  if (!node_)
  {
    return;
  }
  textState_ = node_->props.text;
  if (textState_)
  {
    textState_->bind(&EditTextContext::TextChangedThunk, this, true);
  }
}

void Win32ScenePlatformController::EditTextContext::unbindText()
{
  if (textState_)
  {
    textState_->unbind(&EditTextContext::TextChangedThunk, this);
    textState_ = 0;
  }
}

void Win32ScenePlatformController::EditTextContext::applyText()
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

void Win32ScenePlatformController::EditTextContext::syncStateFromControl()
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
  int len = GetWindowTextLengthA(hwnd_);
  if (len < 0)
  {
    len = 0;
  }
  std::vector<char> buffer(len + 1);
  GetWindowTextA(hwnd_, &buffer[0], len + 1);
  std::string newValue(&buffer[0]);
  if (mutableState->get() != newValue)
  {
    updatingFromControl_ = true;
    mutableState->set(newValue);
    updatingFromControl_ = false;
  }
}

void Win32ScenePlatformController::EditTextContext::TextChangedThunk(void *userData)
{
  EditTextContext *self = static_cast<EditTextContext *>(userData);
  if (self)
  {
    self->applyText();
  }
}
