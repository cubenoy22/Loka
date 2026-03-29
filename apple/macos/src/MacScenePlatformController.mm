#include "MacScenePlatformController.hpp"
#include "MacBuiltInSupport.hpp"
#include "app/scene/node/Boundary.hpp"
#include <AppKit/AppKit.h>
#include <vector>
#include "app/Box.hpp"
#include "app/Grid.hpp"
#include "app/RowColumn.hpp"
#include "app/ZStack.hpp"
#include "app/RectSurface.hpp"
#include "app/layout/ContainerLayout.hpp"
#include "app/layout/LayoutHeuristics.hpp"
#include "app/layout/PlatformBuiltinLayoutHandlers.hpp"
#include "app/scene/Node.hpp"
#include "context/MacEditTextContext.hpp"
#include "context/MacOpenFileDialogContext.hpp"
#include "context/MacRectSurfaceContext.hpp"
#include "loka/core/Profiler.hpp"
#include <map>

namespace
{
  static std::map<void *, MacScenePlatformController *> gControllerByRootView;

  const int kButtonHeight = 32;
  const int kEditTextHeight = 24;
  const int kPopupMenuHeight = 26;
  const int kTextHeight = 20;
  const int kVerticalSpacing = 12;
  const int kHorizontalSpacing = 12;
  const int kImageFallbackHeightModern = 160;

}

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class MacPlatformLayoutTraversal : public IPlatformLayoutTraversal
      {
      public:
        explicit MacPlatformLayoutTraversal(MacScenePlatformController *controller)
            : controller_(controller)
        {
        }

        virtual int layoutChild(Node *child, const LayoutState &state)
        {
          if (!this->controller_)
          {
            return state.y;
          }
          return this->controller_->layoutNodeFromSceneState(child, state);
        }

      private:
        MacScenePlatformController *controller_;
      };
    }
  }
}

MacScenePlatformController::MacScenePlatformController(void *rootView)
    : rootView_(rootView),
      contextMapper_(rootView),
      rootNode_(0),
      lastChangeFlags_(loka::app::scene::NODE_DIRTY_NONE),
      clientWidth_(0),
      clientHeight_(0),
      firstEditField_(0),
      lastEditField_(0),
      focusedEditTextState_(0),
      focusedEditTextControlTag_(0),
      relayoutPending_(false)
{
  RegisterMacBuiltInSupport(*this);
  if (rootView_)
  {
    gControllerByRootView[rootView_] = this;
  }
}

MacScenePlatformController::~MacScenePlatformController()
{
  if (rootView_)
  {
    std::map<void *, MacScenePlatformController *>::iterator it = gControllerByRootView.find(rootView_);
    if (it != gControllerByRootView.end() && it->second == this)
    {
      gControllerByRootView.erase(it);
    }
  }
  clearContexts();
}

bool MacScenePlatformController::registerNodeHandler(loka::app::scene::IPlatformNodeHandler *handler)
{
  return this->nodeHandlerRegistry_.registerHandler(handler);
}

bool MacScenePlatformController::prepareProjectedLayout(loka::app::scene::Node *node,
                                                        loka::app::scene::LayoutState &state)
{
  if (!node)
  {
    return false;
  }
  loka::app::scene::LayoutState handlerState = state;
  if (handlerState.height <= 0)
  {
    handlerState.height = static_cast<short>(kTextHeight);
  }
  loka::app::scene::IPlatformNodeHandler *handler = this->nodeHandlerRegistry_.find(node);
  if (!handler)
  {
    return false;
  }
  return handler->ensureContext(node, this, handlerState) != 0;
}

int MacScenePlatformController::layoutNodeFromSceneState(loka::app::scene::Node *node,
                                                         const loka::app::scene::LayoutState &state)
{
  LayoutState localState;
  localState.x = state.x;
  localState.y = state.y;
  localState.width = state.width;
  localState.height = state.height;
  return this->layoutNode(node, localState);
}

MacScenePlatformController *MacScenePlatformController::findForRootView(void *rootView)
{
  if (!rootView)
  {
    return 0;
  }
  std::map<void *, MacScenePlatformController *>::iterator it = gControllerByRootView.find(rootView);
  if (it == gControllerByRootView.end())
  {
    return 0;
  }
  return it->second;
}

void MacScenePlatformController::onChange(loka::app::scene::Node *rootNode, loka::app::scene::NodeDirtyFlags flags, bool fullRebuild)
{
  rootNode_ = rootNode;
  lastChangeFlags_ = flags;
  relayoutPending_ = false;
  if (!rootView_ || !rootNode_)
  {
    return;
  }

  const bool requiresLayout = (flags & loka::app::scene::NODE_DIRTY_INITIAL) != 0 ||
                              (flags & loka::app::scene::NODE_DIRTY_LAYOUT) != 0 ||
                              (flags & loka::app::scene::NODE_DIRTY_CHILD) != 0;
  if (!requiresLayout)
  {
    return;
  }

  NSView *view = (NSView *)rootView_;
  NSRect bounds = [view bounds];
  clientWidth_ = static_cast<int>(bounds.size.width);
  clientHeight_ = static_cast<int>(bounds.size.height);
  performLayout(clientWidth_, clientHeight_, fullRebuild);
}

void MacScenePlatformController::onBoundaryApply(loka::app::scene::Node *rootNode,
                                                 loka::app::scene::BoundaryNode *boundary,
                                                 const loka::app::scene::BoundaryLocalApplyInfo &info,
                                                 const loka::app::scene::PlatformApplyPlan &plan)
{
  if (rootNode)
  {
    rootNode_ = rootNode;
  }
  if (!rootView_ || !rootNode_ || !boundary || !plan.hasBoundaryApplyWork(boundary))
  {
    return;
  }
  if (info.hasStructureWork || info.hasLayoutWork || !info.hasPaintWork())
  {
    return;
  }

  NSView *view = (NSView *)rootView_;
  if (!info.hasBoundsHint() || info.hasCompositedPaintWork())
  {
    [view setNeedsDisplay:YES];
    return;
  }

  NSRect dirtyRect = NSMakeRect(static_cast<CGFloat>(info.bounds->x),
                                static_cast<CGFloat>(info.bounds->y),
                                static_cast<CGFloat>(info.bounds->width),
                                static_cast<CGFloat>(info.bounds->height));
  [view setNeedsDisplayInRect:dirtyRect];
}

void MacScenePlatformController::synchronize()
{
  // Solid-mode（固定ツリー）では即時反映済みのため、現状何もしない。
}

bool MacScenePlatformController::hasPendingSync() const
{
  return false;
}

void MacScenePlatformController::destroy()
{
  clearContexts();
  rootNode_ = 0;
  lastChangeFlags_ = loka::app::scene::NODE_DIRTY_NONE;
  clientWidth_ = 0;
  clientHeight_ = 0;
  relayoutPending_ = false;
  focusedEditTextState_ = 0;
  focusedEditTextControlTag_ = 0;
}

void MacScenePlatformController::relayout(int clientWidth, int clientHeight)
{
  relayoutPending_ = false;
  if (!rootNode_)
  {
    return;
  }
  if (clientWidth <= 0 || clientHeight <= 0)
  {
    if (rootView_)
    {
      NSView *view = (NSView *)rootView_;
      NSRect bounds = [view bounds];
      clientWidth = static_cast<int>(bounds.size.width);
      clientHeight = static_cast<int>(bounds.size.height);
    }
  }
  clientWidth_ = clientWidth;
  clientHeight_ = clientHeight;
  performLayout(clientWidth_, clientHeight_, false);
}

void MacScenePlatformController::requestRelayout()
{
  if (!rootNode_ || !rootView_)
  {
    return;
  }
  relayoutPending_ = true;
}

void MacScenePlatformController::flushPendingRelayouts()
{
  std::map<void *, MacScenePlatformController *>::iterator it = gControllerByRootView.begin();
  for (; it != gControllerByRootView.end(); ++it)
  {
    MacScenePlatformController *controller = it->second;
    if (!controller || !controller->hasPendingRelayout())
    {
      continue;
    }
    controller->relayout(0, 0);
  }
}

void MacScenePlatformController::performLayout(int clientWidth, int clientHeight, bool rebuildContexts)
{
  if (rebuildContexts)
  {
    captureFocusedEditField();
    // Do NOT clearContexts() here: retained nodes (from local diff) keep
    // their existing context and layoutNode() already creates new contexts
    // for nodes that have none.  Replaced/retired nodes have their contexts
    // released by the composition system via releaseNodeContexts().
  }
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
  if (rebuildContexts)
  {
    restoreFocusedEditField();
  }
}

namespace
{
}

int MacScenePlatformController::applyBoundaryLayoutResult(loka::app::scene::BoundaryNode *boundary,
                                                          int x,
                                                          int y,
                                                          const LayoutNodeResult &result)
{
  if (boundary)
  {
    boundary->setLayoutBounds(x, y, result.boundaryWidth, result.resultY - y);
  }
  return result.resultY;
}

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::layoutRectSurfaceNode(
    loka::app::RectSurfaceNode *surface,
    const LayoutState &state)
{
  MacRectSurfaceContext *ctx = static_cast<MacRectSurfaceContext *>(surface->getContext());
  if (ctx)
  {
    ctx->relayout(state.x, state.y, surface->props.width_, surface->props.height_);
  }
  else
  {
    ctx = new MacRectSurfaceContext(rootView_,
                                    state.x,
                                    state.y,
                                    surface->props.width_,
                                    surface->props.height_,
                                    surface);
    surface->setContext(ctx);
  }
  return LayoutNodeResult(state.width, state.y + surface->props.height_ + kVerticalSpacing);
}

int MacScenePlatformController::layoutNode(loka::app::scene::Node *node, const LayoutState &state)
{
  if (!node)
  {
    return state.y;
  }
  return this->applyBoundaryLayoutResult(node->asBoundary(), state.x, state.y, this->computeLayoutResult(node, state));
}

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::computeLayoutResult(
    loka::app::scene::Node *node,
    const LayoutState &state)
{
  if (loka::app::ColumnNode *column = node->asColumnNode())
  {
    int currentY = state.y;
    loka::app::scene::IPlatformLayoutHandler *handler = this->layoutHandlerRegistry_.find(column);
    if (handler)
    {
      loka::app::scene::LayoutState handlerState;
      handlerState.x = static_cast<short>(state.x);
      handlerState.y = static_cast<short>(state.y);
      handlerState.width = static_cast<short>(state.width);
      handlerState.height = static_cast<short>(state.height);
      loka::app::scene::MacPlatformLayoutTraversal traversal(this);
      currentY = handler->layoutNode(column, handlerState, &traversal);
    }
    else
    {
      currentY = loka::app::layout::computeColumnLayoutResultY(column, state, this, &MacScenePlatformController::layoutContainerChild);
    }
    return LayoutNodeResult(state.width, currentY);
  }

  if (loka::app::RowNode *row = node->asRowNode())
  {
    int maxY = state.y;
    loka::app::scene::IPlatformLayoutHandler *handler = this->layoutHandlerRegistry_.find(row);
    if (handler)
    {
      loka::app::scene::LayoutState handlerState;
      handlerState.x = static_cast<short>(state.x);
      handlerState.y = static_cast<short>(state.y);
      handlerState.width = static_cast<short>(state.width);
      handlerState.height = static_cast<short>(state.height);
      loka::app::scene::MacPlatformLayoutTraversal traversal(this);
      maxY = handler->layoutNode(row, handlerState, &traversal);
    }
    else
    {
      loka::app::layout::RowLayoutMetrics metrics;
      metrics.gap = kHorizontalSpacing;
      metrics.fallbackHeight = kTextHeight;
      metrics.buttonHeight = kButtonHeight;
      metrics.editTextHeight = kEditTextHeight;
      metrics.popupMenuHeight = kPopupMenuHeight;
      metrics.textHeight = kTextHeight;
      metrics.imageFallbackHeight = kImageFallbackHeightModern;
      maxY = loka::app::layout::computeRowLayoutResultY(row, state, metrics, this, &MacScenePlatformController::layoutContainerChild);
    }
    return LayoutNodeResult(state.width, maxY);
  }

  if (loka::app::GridNode *grid = node->asGridNode())
  {
    int maxY = state.y;
    loka::app::scene::IPlatformLayoutHandler *handler = this->layoutHandlerRegistry_.find(grid);
    if (handler)
    {
      loka::app::scene::LayoutState handlerState;
      handlerState.x = static_cast<short>(state.x);
      handlerState.y = static_cast<short>(state.y);
      handlerState.width = static_cast<short>(state.width);
      handlerState.height = static_cast<short>(state.height);
      loka::app::scene::MacPlatformLayoutTraversal traversal(this);
      maxY = handler->layoutNode(grid, handlerState, &traversal);
    }
    else
    {
      loka::app::layout::GridLayoutMetrics metrics;
      metrics.gapX = 0;
      metrics.gapY = 0;
      maxY = loka::app::layout::computeGridLayoutResultY(grid, state, metrics, this, &MacScenePlatformController::layoutContainerChild);
    }
    return LayoutNodeResult(state.width, maxY);
  }

  if (loka::app::BoxNode *box = node->asBoxNode())
  {
    int resultY = state.y;
    loka::app::scene::IPlatformLayoutHandler *handler = this->layoutHandlerRegistry_.find(box);
    if (handler)
    {
      loka::app::scene::LayoutState handlerState;
      handlerState.x = static_cast<short>(state.x);
      handlerState.y = static_cast<short>(state.y);
      handlerState.width = static_cast<short>(state.width);
      handlerState.height = static_cast<short>(state.height);
      loka::app::scene::MacPlatformLayoutTraversal traversal(this);
      resultY = handler->layoutNode(box, handlerState, &traversal);
    }
    else
    {
      resultY = loka::app::layout::computeBoxLayoutResultY(box, state, this, &MacScenePlatformController::layoutContainerChild);
    }
    return LayoutNodeResult(state.width, resultY);
  }

  if (loka::app::ZStackNode *stack = node->asZStackNode())
  {
    int maxY = state.y;
    loka::app::scene::IPlatformLayoutHandler *handler = this->layoutHandlerRegistry_.find(stack);
    if (handler)
    {
      loka::app::scene::LayoutState handlerState;
      handlerState.x = static_cast<short>(state.x);
      handlerState.y = static_cast<short>(state.y);
      handlerState.width = static_cast<short>(state.width);
      handlerState.height = static_cast<short>(state.height);
      loka::app::scene::MacPlatformLayoutTraversal traversal(this);
      maxY = handler->layoutNode(stack, handlerState, &traversal);
    }
    else
    {
      maxY = loka::app::layout::computeZStackLayoutResultY(stack, state, this, &MacScenePlatformController::layoutContainerChild);
    }
    return LayoutNodeResult(state.width, maxY);
  }

  if (loka::app::scene::INestable *nestable = node->asNestable())
  {
    LayoutState childState = state;
    loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
    for (loka::app::scene::Node *child = it.next(); child; child = it.next())
    {
      childState.y = layoutNode(child, childState);
    }
    return LayoutNodeResult(state.width, childState.y);
  }

  if (loka::app::scene::IProjectedLayoutNode *projected = node->asProjectedLayoutNode())
  {
    loka::app::scene::LayoutState projectedState;
    projectedState.x = static_cast<short>(state.x);
    projectedState.y = static_cast<short>(state.y);
    projectedState.width = static_cast<short>(state.width);
    projectedState.height = static_cast<short>(state.height);
    projectedState.lineHeight = 0;
    projectedState.spacing = 0;
    return LayoutNodeResult(state.width, projected->layoutProjected(this, projectedState));
  }

  LeafLayoutHandlerFn leafLayoutHandler = this->leafLayoutHandlerRegistry_.find(node);
  if (leafLayoutHandler)
  {
    return leafLayoutHandler(this, node, state);
  }

  LeafLayoutHandlerFn hostActionHandler = this->hostActionHandlerRegistry_.find(node);
  if (hostActionHandler)
  {
    return hostActionHandler(this, node, state);
  }

  if (loka::app::RectSurfaceNode *surface = node->asRectSurfaceNode())
  {
    return this->layoutRectSurfaceNode(surface, state);
  }

  return LayoutNodeResult(state.width, state.y);
}

int MacScenePlatformController::layoutContainerChild(void *context, loka::app::scene::Node *child, const LayoutState &state)
{
  MacScenePlatformController *controller = static_cast<MacScenePlatformController *>(context);
  if (!controller)
  {
    return state.y;
  }
  return controller->layoutNode(child, state);
}

void MacScenePlatformController::registerEditField(void *field)
{
  if (!field)
  {
    return;
  }
  NSTextField *textField = (NSTextField *)field;
  if (!firstEditField_)
  {
    firstEditField_ = field;
  }
  if (lastEditField_)
  {
    NSTextField *lastField = (NSTextField *)lastEditField_;
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
  NSTextField *firstField = (NSTextField *)firstEditField_;
  NSTextField *lastField = (NSTextField *)lastEditField_;
  [lastField setNextKeyView:firstField];

  if (rootView_)
  {
    NSView *view = (NSView *)rootView_;
    NSWindow *window = [view window];
    if (window)
    {
      [window setInitialFirstResponder:firstField];
    }
  }
}

void MacScenePlatformController::captureFocusedEditField()
{
  focusedEditTextState_ = 0;
  focusedEditTextControlTag_ = 0;
  if (!rootNode_ || !rootView_)
  {
    return;
  }
  void *state = findFocusedEditTextState(rootNode_);
  if (!state)
  {
    return;
  }
  focusedEditTextState_ = state;
}

void *MacScenePlatformController::findFocusedEditTextState(loka::app::scene::Node *node) const
{
  if (!node)
  {
    return 0;
  }
  if (loka::app::EditTextNode *edit = node->asEditTextNode())
  {
    MacEditTextContext *ctx = static_cast<MacEditTextContext *>(edit->getContext());
    NSTextField *field = ctx ? (NSTextField *)ctx->nativeField() : 0;
    if (field && [field currentEditor] != nil)
    {
      if (edit->props.controlTag_ != 0)
      {
        const_cast<MacScenePlatformController *>(this)->focusedEditTextControlTag_ = edit->props.controlTag_;
      }
      return edit->props.text_;
    }
  }
  if (loka::app::scene::INestable *nestable = node->asNestable())
  {
    loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
    for (loka::app::scene::Node *child = it.next(); child; child = it.next())
    {
      void *state = findFocusedEditTextState(child);
      if (state)
      {
        return state;
      }
    }
  }
  return 0;
}

void *MacScenePlatformController::findFieldForFocusedEdit(loka::app::scene::Node *node) const
{
  if (!node)
  {
    return 0;
  }
  if (loka::app::EditTextNode *edit = node->asEditTextNode())
  {
    const bool controlTagMatches = focusedEditTextControlTag_ != 0 && edit->props.controlTag_ == focusedEditTextControlTag_;
    const bool stateMatches = focusedEditTextState_ != 0 && edit->props.text_ == focusedEditTextState_;
    if (controlTagMatches || (focusedEditTextControlTag_ == 0 && stateMatches))
    {
      MacEditTextContext *ctx = static_cast<MacEditTextContext *>(edit->getContext());
      return ctx ? ctx->nativeField() : 0;
    }
  }
  if (loka::app::scene::INestable *nestable = node->asNestable())
  {
    loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
    for (loka::app::scene::Node *child = it.next(); child; child = it.next())
    {
      void *field = findFieldForFocusedEdit(child);
      if (field)
      {
        return field;
      }
    }
  }
  return 0;
}

void MacScenePlatformController::restoreFocusedEditField()
{
  if (!focusedEditTextState_ || !rootView_ || !rootNode_)
  {
    focusedEditTextState_ = 0;
    focusedEditTextControlTag_ = 0;
    return;
  }
  NSView *view = (NSView *)rootView_;
  NSWindow *window = [view window];
  if (!window)
  {
    focusedEditTextState_ = 0;
    focusedEditTextControlTag_ = 0;
    return;
  }
  NSTextField *field = (NSTextField *)findFieldForFocusedEdit(rootNode_);
  if (field)
  {
    [window makeFirstResponder:field];
  }
  focusedEditTextState_ = 0;
  focusedEditTextControlTag_ = 0;
}

void MacScenePlatformController::releaseNodeContexts(loka::app::scene::Node *node)
{
  clearNodeContexts(node);
}

void MacScenePlatformController::clearContexts()
{
  clearNodeContexts(rootNode_);
}

void MacScenePlatformController::clearNodeContexts(loka::app::scene::Node *node)
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

int MacScenePlatformController::measureClientWidth(int requestedWidth) const
{
  if (requestedWidth > 0)
  {
    return requestedWidth;
  }
  if (rootView_)
  {
    NSView *view = (NSView *)rootView_;
    NSRect bounds = [view bounds];
    return static_cast<int>(bounds.size.width);
  }
  return 260;
}
