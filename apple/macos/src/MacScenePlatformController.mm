#include "MacScenePlatformController.hpp"
#include <AppKit/AppKit.h>
#include <vector>
#include "app/Box.hpp"
#include "app/Button.hpp"
#include "app/EditText.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "core2/scene/Node.hpp"
#include "context/MacButtonContext.hpp"
#include "context/MacEditTextContext.hpp"
#include "context/MacTextContext.hpp"

namespace
{
  const int kButtonHeight = 32;
  const int kEditTextHeight = 24;
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
  layoutNode(rootNode_, state);
  finalizeKeyLoop();
}

int MacScenePlatformController::layoutNode(declara::core::scene::Node *node, const LayoutState &state)
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
    MacButtonContext *ctx = new MacButtonContext(rootView_, state.x, state.y, state.width, kButtonHeight, button);
    button->setContext(ctx);

    LayoutState nextState = state;
    nextState.y = state.y + kButtonHeight + kVerticalSpacing;
    return nextState.y;
  }

  if (declara::app::EditTextNode *edit = dynamic_cast<declara::app::EditTextNode *>(node))
  {
    MacEditTextContext *ctx = new MacEditTextContext(rootView_, state.x, state.y, state.width, kEditTextHeight, edit);
    edit->setContext(ctx);
    registerEditField(ctx->nativeField());

    LayoutState nextState = state;
    nextState.y = state.y + kEditTextHeight + kVerticalSpacing;
    return nextState.y;
  }

  if (declara::app::TextNode *text = dynamic_cast<declara::app::TextNode *>(node))
  {
    MacTextContext *ctx = new MacTextContext(rootView_, state.x, state.y, state.width, kTextHeight, text);
    text->setContext(ctx);

    LayoutState nextState = state;
    nextState.y = state.y + kTextHeight + kVerticalSpacing;
    return nextState.y;
  }

  return state.y;
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
  if (declara::core::scene::INestable *nestable = dynamic_cast<declara::core::scene::INestable *>(node))
  {
    const std::vector<declara::core::scene::Node *> &children = nestable->getChildren();
    for (size_t i = 0; i < children.size(); ++i)
    {
      clearNodeContexts(children[i]);
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
