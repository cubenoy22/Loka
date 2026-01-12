#include "MacScenePlatformController.hpp"
#include "core2/scene/node/Boundary.hpp"
#include <AppKit/AppKit.h>
#include <vector>
#include "app/Box.hpp"
#include "app/Button.hpp"
#include "app/EditText.hpp"
#include "app/PopupMenu.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "core2/scene/Node.hpp"
#include "context/MacButtonContext.hpp"
#include "context/MacEditTextContext.hpp"
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

void MacScenePlatformController::onChange(declara::core::scene::Node *rootNode, declara::core::scene::NodeDirtyFlags flags)
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
  int ApplyBoundaryBounds(declara::core::scene::BoundaryNode *boundary,
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

int MacScenePlatformController::layoutNode(declara::core::scene::Node *node, const LayoutState &state)
{
  if (!node)
  {
    return state.y;
  }
  declara::core::scene::BoundaryNode *boundary = node->asBoundary();
  const int startX = state.x;
  const int startY = state.y;
  const int startWidth = state.width;

  if (declara::app::RowNode *row = node->asRowNode())
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
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, maxY);
  }

  if (declara::core::scene::INestable *nestable = node->asNestable())
  {
    LayoutState childState = state;
    loka::dsl::CompositionCursor<declara::core::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
    for (declara::core::scene::Node *child = it.next(); child; child = it.next())
    {
      childState.y = layoutNode(child, childState);
    }
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, childState.y);
  }

  if (declara::app::ButtonNode *button = node->asButtonNode())
  {
    MacButtonContext *ctx = new MacButtonContext(rootView_, state.x, state.y, state.width, kButtonHeight, button);
    button->setContext(ctx);

    LayoutState nextState = state;
    nextState.y = state.y + kButtonHeight + kVerticalSpacing;
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, nextState.y);
  }

  if (declara::app::EditTextNode *edit = node->asEditTextNode())
  {
    MacEditTextContext *ctx = new MacEditTextContext(rootView_, state.x, state.y, state.width, kEditTextHeight, edit);
    edit->setContext(ctx);
    registerEditField(ctx->nativeField());

    LayoutState nextState = state;
    nextState.y = state.y + kEditTextHeight + kVerticalSpacing;
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, nextState.y);
  }

  if (declara::app::PopupMenuNode *popup = node->asPopupMenuNode())
  {
    MacPopupMenuContext *ctx = new MacPopupMenuContext(rootView_, state.x, state.y, state.width, kPopupMenuHeight, popup);
    popup->setContext(ctx);

    LayoutState nextState = state;
    nextState.y = state.y + kPopupMenuHeight + kVerticalSpacing;
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, nextState.y);
  }

  if (declara::app::TextNode *text = node->asTextNode())
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

void MacScenePlatformController::clearNodeContexts(declara::core::scene::Node *node)
{
  if (!node)
  {
    return;
  }
  if (declara::core::scene::INestable *nestable = node->asNestable())
  {
    loka::dsl::CompositionCursor<declara::core::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
    for (declara::core::scene::Node *child = it.next(); child; child = it.next())
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
