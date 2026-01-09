#include "Win32ScenePlatformController.hpp"
#include <windows.h>
#include <vector>
#include "app/Box.hpp"
#include "app/Button.hpp"
#include "app/EditText.hpp"
#include "app/PopupMenu.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "core2/scene/Node.hpp"

namespace
{
  const int kButtonHeight = 32;
  const int kEditTextHeight = 24;
  const int kPopupMenuHeight = 26;
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

void Win32ScenePlatformController::onChange(declara::core::scene::Node *rootNode, declara::core::scene::NodeDirtyFlags flags)
{
  (void)flags;
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
    std::map<HWND, Win32ButtonContext *>::iterator it = buttonMap_.find(target);
    if (it == buttonMap_.end())
    {
      return false;
    }
    return it->second->handleCommand(wParam, lParam);
  }
  if (code == EN_CHANGE)
  {
    std::map<HWND, Win32EditTextContext *>::iterator itEdit = editMap_.find(target);
    if (itEdit == editMap_.end())
    {
      return false;
    }
    return itEdit->second->handleCommand(wParam, lParam);
  }
  if (code == CBN_SELCHANGE)
  {
    std::map<HWND, Win32PopupMenuContext *>::iterator itPopup = popupMap_.find(target);
    if (itPopup == popupMap_.end())
    {
      return false;
    }
    return itPopup->second->handleCommand(wParam, lParam);
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
    size_t childCount = row->childrenCount();
    if (row->childrenHead() == 0 || childCount == 0)
    {
      return state.y;
    }
    LayoutState childState = state;
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
    loka::dsl::CompositionCursor<declara::core::scene::Node> it(row->childrenHead(), childCount);
    for (declara::core::scene::Node *child = it.next(); child; child = it.next())
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
      int childY = layoutNode(child, childState);
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
    loka::dsl::CompositionCursor<declara::core::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
    for (declara::core::scene::Node *child = it.next(); child; child = it.next())
    {
      childState.y = layoutNode(child, childState);
    }
    return childState.y;
  }

  if (declara::app::ButtonNode *button = dynamic_cast<declara::app::ButtonNode *>(node))
  {
    Win32ButtonContext *ctx = new Win32ButtonContext(rootHwnd_, state.x, state.y, state.width, kButtonHeight, button);
    button->setContext(ctx);
    buttonMap_[ctx->hwnd()] = ctx;

    LayoutState nextState = state;
    nextState.y = state.y + kButtonHeight + kVerticalSpacing;
    return nextState.y;
  }

  if (declara::app::EditTextNode *edit = dynamic_cast<declara::app::EditTextNode *>(node))
  {
    Win32EditTextContext *ctx = new Win32EditTextContext(rootHwnd_, state.x, state.y, state.width, kEditTextHeight, edit);
    edit->setContext(ctx);
    editMap_[ctx->hwnd()] = ctx;

    LayoutState nextState = state;
    nextState.y = state.y + kEditTextHeight + kVerticalSpacing;
    return nextState.y;
  }

  if (declara::app::PopupMenuNode *popup = dynamic_cast<declara::app::PopupMenuNode *>(node))
  {
    Win32PopupMenuContext *ctx = new Win32PopupMenuContext(rootHwnd_, state.x, state.y, state.width, kPopupMenuHeight, popup);
    popup->setContext(ctx);
    popupMap_[ctx->hwnd()] = ctx;

    LayoutState nextState = state;
    nextState.y = state.y + kPopupMenuHeight + kVerticalSpacing;
    return nextState.y;
  }

  if (declara::app::TextNode *text = dynamic_cast<declara::app::TextNode *>(node))
  {
    Win32TextContext *ctx = new Win32TextContext(rootHwnd_, state.x, state.y, state.width, kTextHeight, text);
    text->setContext(ctx);

    LayoutState nextState = state;
    nextState.y = state.y + kTextHeight + kVerticalSpacing;
    return nextState.y;
  }

  return state.y;
}

void Win32ScenePlatformController::clearContexts()
{
  buttonMap_.clear();
  editMap_.clear();
  popupMap_.clear();
  clearNodeContexts(rootNode_);
}

void Win32ScenePlatformController::clearNodeContexts(declara::core::scene::Node *node)
{
  if (!node)
  {
    return;
  }
  if (declara::core::scene::INestable *nestable = dynamic_cast<declara::core::scene::INestable *>(node))
  {
    loka::dsl::CompositionCursor<declara::core::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
    for (declara::core::scene::Node *child = it.next(); child; child = it.next())
    {
      clearNodeContexts(child);
    }
  }
  node->setContext(0);
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
