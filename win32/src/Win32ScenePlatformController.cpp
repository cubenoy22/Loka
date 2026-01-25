#include "Win32ScenePlatformController.hpp"
#include "app/scene/node/Boundary.hpp"
#include <windows.h>
#include <vector>
#include "app/Box.hpp"
#include "app/Button.hpp"
#include "app/Cell.hpp"
#include "app/EditText.hpp"
#include "app/Grid.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/PopupMenu.hpp"
#include "app/RowColumn.hpp"
#include "app/ZStack.hpp"
#include "app/Text.hpp"
#include "app/scene/Node.hpp"
#include "loka/core/Profiler.hpp"
#include "context/Win32CellContext.hpp"

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

void Win32ScenePlatformController::onChange(loka::app::scene::Node *rootNode, loka::app::scene::NodeDirtyFlags flags)
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
  PROFILE_SECTION("layout");
  layoutNode(rootNode_, state);
}

namespace
{
  int ApplyBoundaryBounds(loka::app::scene::BoundaryNode *boundary,
                          int x,
                          int y,
                          int width,
                          int resultY)
  {
    if (boundary)
    {
      boundary->setLayoutBounds(x, y, width, resultY - y);
    }
    return resultY;
  }
}

int Win32ScenePlatformController::layoutNode(loka::app::scene::Node *node, const LayoutState &state)
{
  if (!node)
  {
    return state.y;
  }
  loka::app::scene::BoundaryNode *boundary = node->asBoundary();
  const int startX = state.x;
  const int startY = state.y;
  const int startWidth = state.width;

  if (loka::app::RowNode *row = node->asRowNode())
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
    loka::dsl::CompositionCursor<loka::app::scene::Node> it(row->childrenHead(), childCount);
    for (loka::app::scene::Node *child = it.next(); child; child = it.next())
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
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, maxY);
  }

  if (loka::app::GridNode *grid = node->asGridNode())
  {
    const int rows = grid->props.rows > 0 ? grid->props.rows : 1;
    const int cols = grid->props.cols > 0 ? grid->props.cols : 1;
    const int gapX = 0;
    const int gapY = 0;
    const int availableWidth = state.width - gapX * (cols - 1);
    const int availableHeight = state.height - gapY * (rows - 1);
    const int cellWidth = availableWidth > 0 ? availableWidth / cols : 0;
    const int cellHeight = availableHeight > 0 ? availableHeight / rows : 0;
    int maxY = state.y;
    if (loka::app::scene::INestable *nestable = grid->asNestable())
    {
      const size_t childCount = nestable->childrenCount();
      const size_t maxCount = static_cast<size_t>(rows * cols);
      size_t index = 0;
      loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), childCount);
      for (loka::app::scene::Node *child = it.next(); child && index < maxCount; child = it.next(), ++index)
      {
        const int row = static_cast<int>(index / cols);
        const int col = static_cast<int>(index % cols);
        LayoutState childState = state;
        childState.x = state.x + col * (cellWidth + gapX);
        childState.y = state.y + row * (cellHeight + gapY);
        childState.width = cellWidth;
        childState.height = cellHeight;
        int childY = layoutNode(child, childState);
        if (childY > maxY)
        {
          maxY = childY;
        }
      }
    }
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, maxY);
  }

  if (loka::app::BoxNode *box = node->asBoxNode())
  {
    int padding = box->props.padding;
    LayoutState childState = state;
    childState.x = state.x + padding;
    childState.y = state.y + padding;
    childState.width = state.width - padding * 2;
    if (childState.width < 0)
    {
      childState.width = 0;
    }
    childState.height = state.height - padding * 2;
    if (childState.height < 0)
    {
      childState.height = 0;
    }
    int resultY = childState.y;
    if (loka::app::scene::INestable *nestable = box->asNestable())
    {
      loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
      for (loka::app::scene::Node *child = it.next(); child; child = it.next())
      {
        childState.y = layoutNode(child, childState);
      }
      resultY = childState.y + padding;
    }
    else
    {
      resultY = state.y + padding * 2;
    }
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, resultY);
  }

  if (loka::app::ZStackNode *stack = node->asZStackNode())
  {
    LayoutState childState = state;
    int maxY = state.y;
    if (loka::app::scene::INestable *nestable = stack->asNestable())
    {
      loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
      for (loka::app::scene::Node *child = it.next(); child; child = it.next())
      {
        childState = state;
        int childY = layoutNode(child, childState);
        if (childY > maxY)
        {
          maxY = childY;
        }
      }
    }
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, maxY);
  }

  if (loka::app::scene::INestable *nestable = node->asNestable())
  {
    LayoutState childState = state;
    loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
    for (loka::app::scene::Node *child = it.next(); child; child = it.next())
    {
      childState.y = layoutNode(child, childState);
    }
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, childState.y);
  }

  if (loka::app::OpenFileDialogNode *dialog = node->asOpenFileDialogNode())
  {
    Win32OpenFileDialogContext *ctx = new Win32OpenFileDialogContext(rootHwnd_, dialog);
    dialog->setContext(ctx);
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, state.y);
  }

  if (loka::app::ButtonNode *button = node->asButtonNode())
  {
    Win32ButtonContext *ctx = new Win32ButtonContext(rootHwnd_, state.x, state.y, state.width, kButtonHeight, button);
    button->setContext(ctx);
    buttonMap_[ctx->hwnd()] = ctx;

    LayoutState nextState = state;
    nextState.y = state.y + kButtonHeight + kVerticalSpacing;
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, nextState.y);
  }

  if (loka::app::EditTextNode *edit = node->asEditTextNode())
  {
    Win32EditTextContext *ctx = new Win32EditTextContext(rootHwnd_, state.x, state.y, state.width, kEditTextHeight, edit);
    edit->setContext(ctx);
    editMap_[ctx->hwnd()] = ctx;

    LayoutState nextState = state;
    nextState.y = state.y + kEditTextHeight + kVerticalSpacing;
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, nextState.y);
  }

  if (loka::app::PopupMenuNode *popup = node->asPopupMenuNode())
  {
    Win32PopupMenuContext *ctx = new Win32PopupMenuContext(rootHwnd_, state.x, state.y, state.width, kPopupMenuHeight, popup);
    popup->setContext(ctx);
    popupMap_[ctx->hwnd()] = ctx;

    LayoutState nextState = state;
    nextState.y = state.y + kPopupMenuHeight + kVerticalSpacing;
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, nextState.y);
  }

  if (loka::app::CellNode *cell = node->asCellNode())
  {
    const int cellHeight = state.height > 0 ? state.height : kTextHeight;
    Win32CellContext *ctx = new Win32CellContext(rootHwnd_, state.x, state.y, state.width, cellHeight, cell);
    cell->setContext(ctx);

    LayoutState nextState = state;
    nextState.y = state.y + cellHeight;
    if (state.height <= 0)
    {
      nextState.y += kVerticalSpacing;
    }
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, nextState.y);
  }

  if (loka::app::TextNode *text = node->asTextNode())
  {
    Win32TextContext *ctx = new Win32TextContext(rootHwnd_, state.x, state.y, state.width, kTextHeight, text);
    text->setContext(ctx);

    LayoutState nextState = state;
    nextState.y = state.y + kTextHeight + kVerticalSpacing;
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, nextState.y);
  }

  return ApplyBoundaryBounds(boundary, startX, startY, startWidth, state.y);
}

void Win32ScenePlatformController::clearContexts()
{
  buttonMap_.clear();
  editMap_.clear();
  popupMap_.clear();
  clearNodeContexts(rootNode_);
}

void Win32ScenePlatformController::clearNodeContexts(loka::app::scene::Node *node)
{
  if (!node)
  {
    return;
  }
  if (loka::app::scene::INestable *nestable = node->asNestable())
  {
    loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
    for (loka::app::scene::Node *child = it.next(); child; child = it.next())
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
