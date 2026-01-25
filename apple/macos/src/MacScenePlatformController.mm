#include "MacScenePlatformController.hpp"
#include "core2/scene/node/Boundary.hpp"
#include <AppKit/AppKit.h>
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
#include "core2/scene/Node.hpp"
#include "context/MacButtonContext.hpp"
#include "context/MacCellContext.hpp"
#include "context/MacEditTextContext.hpp"
#include "context/MacOpenFileDialogContext.hpp"
#include "context/MacTextContext.hpp"
#include "context/MacPopupMenuContext.hpp"
#include "core/Profiler.hpp"

namespace
{
  const int kButtonHeight = 32;
  const int kEditTextHeight = 24;
  const int kPopupMenuHeight = 26;
  const int kTextHeight = 20;
  const int kVerticalSpacing = 12;
  const int kHorizontalSpacing = 12;
}

MacScenePlatformController::MacScenePlatformController(void *rootView)
    : rootView_(rootView),
      rootNode_(0),
      clientWidth_(0),
      clientHeight_(0),
      firstEditField_(0),
      lastEditField_(0)
{
}

MacScenePlatformController::~MacScenePlatformController()
{
  clearContexts();
}

void MacScenePlatformController::onChange(loka::core::scene::Node *rootNode, loka::core::scene::NodeDirtyFlags flags)
{
  (void)flags;
  rootNode_ = rootNode;
  if (!rootView_ || !rootNode_)
  {
    return;
  }

  NSView *view = (__bridge NSView *)rootView_;
  NSRect bounds = [view bounds];
  clientWidth_ = static_cast<int>(bounds.size.width);
  clientHeight_ = static_cast<int>(bounds.size.height);
  performLayout(clientWidth_, clientHeight_);
}

void MacScenePlatformController::synchronize()
{
  // Solid-mode（固定ツリー）では即時反映済みのため、現状何もしない。
}

void MacScenePlatformController::destroy()
{
  clearContexts();
  rootNode_ = 0;
  clientWidth_ = 0;
  clientHeight_ = 0;
}

void MacScenePlatformController::relayout(int clientWidth, int clientHeight)
{
  if (!rootNode_)
  {
    return;
  }
  if (clientWidth <= 0 || clientHeight <= 0)
  {
    if (rootView_)
    {
      NSView *view = (__bridge NSView *)rootView_;
      NSRect bounds = [view bounds];
      clientWidth = static_cast<int>(bounds.size.width);
      clientHeight = static_cast<int>(bounds.size.height);
    }
  }
  clientWidth_ = clientWidth;
  clientHeight_ = clientHeight;
  performLayout(clientWidth_, clientHeight_);
}

void MacScenePlatformController::performLayout(int clientWidth, int clientHeight)
{
  clearContexts();
  if (!rootNode_ || !rootView_)
  {
    return;
  }
  firstEditField_ = 0;
  lastEditField_ = 0;
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
  finalizeKeyLoop();
}

namespace
{
  int ApplyBoundaryBounds(loka::core::scene::BoundaryNode *boundary,
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

int MacScenePlatformController::layoutNode(loka::core::scene::Node *node, const LayoutState &state)
{
  if (!node)
  {
    return state.y;
  }
  loka::core::scene::BoundaryNode *boundary = node->asBoundary();
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
    loka::dsl::CompositionCursor<loka::core::scene::Node> it(row->childrenHead(), childCount);
    for (loka::core::scene::Node *child = it.next(); child; child = it.next())
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
    if (loka::core::scene::INestable *nestable = grid->asNestable())
    {
      const size_t childCount = nestable->childrenCount();
      const size_t maxCount = static_cast<size_t>(rows * cols);
      size_t index = 0;
      loka::dsl::CompositionCursor<loka::core::scene::Node> it(nestable->childrenHead(), childCount);
      for (loka::core::scene::Node *child = it.next(); child && index < maxCount; child = it.next(), ++index)
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
    if (loka::core::scene::INestable *nestable = box->asNestable())
    {
      loka::dsl::CompositionCursor<loka::core::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
      for (loka::core::scene::Node *child = it.next(); child; child = it.next())
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
    if (loka::core::scene::INestable *nestable = stack->asNestable())
    {
      loka::dsl::CompositionCursor<loka::core::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
      for (loka::core::scene::Node *child = it.next(); child; child = it.next())
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

  if (loka::core::scene::INestable *nestable = node->asNestable())
  {
    LayoutState childState = state;
    loka::dsl::CompositionCursor<loka::core::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
    for (loka::core::scene::Node *child = it.next(); child; child = it.next())
    {
      childState.y = layoutNode(child, childState);
    }
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, childState.y);
  }

  if (loka::app::OpenFileDialogNode *dialog = node->asOpenFileDialogNode())
  {
    MacOpenFileDialogContext *ctx = new MacOpenFileDialogContext(rootView_, dialog);
    dialog->setContext(ctx);
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, state.y);
  }

  if (loka::app::ButtonNode *button = node->asButtonNode())
  {
    MacButtonContext *ctx = new MacButtonContext(rootView_, state.x, state.y, state.width, kButtonHeight, button);
    button->setContext(ctx);

    LayoutState nextState = state;
    nextState.y = state.y + kButtonHeight + kVerticalSpacing;
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, nextState.y);
  }

  if (loka::app::EditTextNode *edit = node->asEditTextNode())
  {
    MacEditTextContext *ctx = new MacEditTextContext(rootView_, state.x, state.y, state.width, kEditTextHeight, edit);
    edit->setContext(ctx);
    registerEditField(ctx->nativeField());

    LayoutState nextState = state;
    nextState.y = state.y + kEditTextHeight + kVerticalSpacing;
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, nextState.y);
  }

  if (loka::app::PopupMenuNode *popup = node->asPopupMenuNode())
  {
    MacPopupMenuContext *ctx = new MacPopupMenuContext(rootView_, state.x, state.y, state.width, kPopupMenuHeight, popup);
    popup->setContext(ctx);

    LayoutState nextState = state;
    nextState.y = state.y + kPopupMenuHeight + kVerticalSpacing;
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, nextState.y);
  }

  if (loka::app::CellNode *cell = node->asCellNode())
  {
    const int cellHeight = state.height > 0 ? state.height : kTextHeight;
    MacCellContext *ctx = new MacCellContext(rootView_, state.x, state.y, state.width, cellHeight, cell);
    cell->setContext(ctx);

    LayoutState nextState = state;
    nextState.y = state.y + cellHeight;
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, nextState.y);
  }

  if (loka::app::TextNode *text = node->asTextNode())
  {
    MacTextContext *ctx = new MacTextContext(rootView_, state.x, state.y, state.width, kTextHeight, text);
    text->setContext(ctx);

    LayoutState nextState = state;
    nextState.y = state.y + kTextHeight + kVerticalSpacing;
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, nextState.y);
  }

  return ApplyBoundaryBounds(boundary, startX, startY, startWidth, state.y);
}

void MacScenePlatformController::registerEditField(void *field)
{
  if (!field)
  {
    return;
  }
  NSTextField *textField = (__bridge NSTextField *)field;
  if (!firstEditField_)
  {
    firstEditField_ = field;
  }
  if (lastEditField_)
  {
    NSTextField *lastField = (__bridge NSTextField *)lastEditField_;
    [lastField setNextKeyView:textField];
  }
  lastEditField_ = field;
}

void MacScenePlatformController::finalizeKeyLoop()
{
  if (!firstEditField_ || !lastEditField_)
  {
    return;
  }
  if (firstEditField_ == lastEditField_)
  {
    return;
  }
  NSTextField *firstField = (__bridge NSTextField *)firstEditField_;
  NSTextField *lastField = (__bridge NSTextField *)lastEditField_;
  [lastField setNextKeyView:firstField];

  if (rootView_)
  {
    NSView *view = (__bridge NSView *)rootView_;
    NSWindow *window = [view window];
    if (window)
    {
      [window setInitialFirstResponder:firstField];
    }
  }
}

void MacScenePlatformController::clearContexts()
{
  clearNodeContexts(rootNode_);
}

void MacScenePlatformController::clearNodeContexts(loka::core::scene::Node *node)
{
  if (!node)
  {
    return;
  }
  if (loka::core::scene::INestable *nestable = node->asNestable())
  {
    loka::dsl::CompositionCursor<loka::core::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
    for (loka::core::scene::Node *child = it.next(); child; child = it.next())
    {
      clearNodeContexts(child);
    }
  }
  node->setContext(0);
}

int MacScenePlatformController::measureClientWidth(int requestedWidth) const
{
  if (requestedWidth > 0)
  {
    return requestedWidth;
  }
  if (rootView_)
  {
    NSView *view = (__bridge NSView *)rootView_;
    NSRect bounds = [view bounds];
    return static_cast<int>(bounds.size.width);
  }
  return 260;
}
