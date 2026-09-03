#include "MacScenePlatformController.hpp"
#include "MacBuiltInSupport.hpp"
#include "MacObjCCompat.hpp"
#include "app/scene/boundary/Boundary.hpp"
#include <cassert>
#include <climits>
#include <AppKit/AppKit.h>
#include <vector>
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/nestable/Grid.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/ScrollView.hpp"
#include "app/nodes/nestable/ZStack.hpp"
#include "app/RectSurface.hpp"
#include "app/layout/FallbackControlMetrics.hpp"
#include "app/layout/BoxLayout.hpp"
#include "app/layout/ColumnLayout.hpp"
#include "app/layout/GridLayout.hpp"
#include "app/layout/RowLayout.hpp"
#include "app/layout/ZStackLayout.hpp"
#include "app/layout/LayoutHeuristics.hpp"
#include "app/layout/PlatformBuiltinLayoutHandlers.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/scene/Node.hpp"
#include "context/MacEditTextContext.hpp"
#include "context/MacOpenFileDialogContext.hpp"
#include "context/MacRectSurfaceContext.hpp"
#include "context/MacScrollViewContext.hpp"
#include "core/Profiler.hpp"
#include <map>

namespace
{
  static std::map<void *, MacScenePlatformController *> gControllerByRootView;

} // namespace

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
            : controller_(controller),
              layoutResultY_(0)
        {
        }

        virtual int layoutChild(Node *child, const LayoutState &state)
        {
          if (!this->controller_)
          {
            return state.y;
          }
          const int result = this->controller_->layoutNodeFromSceneState(child, state);
          if (this->controller_->refuseNarrowingInScrollScope(result))
          {
            return state.y;
          }
          this->layoutResultY_ = static_cast<short>(result);
          return result;
        }

        virtual void setLayoutResultY(short y)
        {
          this->layoutResultY_ = y;
        }

        virtual short layoutResultY() const
        {
          return this->layoutResultY_;
        }

      private:
        MacScenePlatformController *controller_;
        short layoutResultY_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

MacScenePlatformController::MacScenePlatformController(void *rootView)
    : rootView_(rootView),
      projectionParentScopes_(rootView),
      rootNode_(0),
      rectSurfaceExtentLedger_(),
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
  this->drainNativeRetirements();
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
    handlerState.height = static_cast<short>(loka::app::layout::FallbackControlMetrics::kTextHeight);
  }
  loka::app::scene::IPlatformNodeHandler *handler = this->nodeHandlerRegistry_.find(node);
  if (!handler)
  {
    assert(false && "no node handler registered for this node type -- register the handler or an explicit RefusedNodeHandler");
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

void MacScenePlatformController::onChange(loka::app::scene::Node *rootNode,
                                          loka::app::scene::NodeDirtyFlags flags,
                                          bool fullRebuild)
{
  rootNode_ = rootNode;
  lastChangeFlags_ = flags;
  relayoutPending_ = false;
  if (!rootView_ || !rootNode_)
  {
    return;
  }

  const bool requiresLayout = (flags & loka::app::scene::NODE_DIRTY_INITIAL) != 0
                              || (flags & loka::app::scene::NODE_DIRTY_LAYOUT) != 0
                              || (flags & loka::app::scene::NODE_DIRTY_CHILD) != 0;
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
  return !this->nativeRetirements_.empty();
}

void MacScenePlatformController::queueNativeRetirement(void *primary, void *auxiliary)
{
  if (primary || auxiliary)
  {
    this->nativeRetirements_.push_back(NativeRetirement(primary, auxiliary));
  }
}

void MacScenePlatformController::drainNativeRetirements()
{
  for (size_t i = 0; i < this->nativeRetirements_.size(); ++i)
  {
    NativeRetirement &entry = this->nativeRetirements_[i];
    if (entry.auxiliary)
    {
      [(id)entry.auxiliary release];
    }
    if (entry.primary)
    {
      [(id)entry.primary release];
    }
  }
  this->nativeRetirements_.clear();
}

void MacScenePlatformController::destroy()
{
  clearContexts();
  this->drainNativeRetirements();
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
  assert(this->projectionParentScopes_.activeDepth() == 0 &&
         "a macOS projection pass must begin at the root scope");
  const loka::core::Frame rootClip(0, 0, clientWidth, clientHeight);
  if (!this->projectionParentScopes_.resetRoot(this->rootView_, rootClip))
  {
    return;
  }
  PROFILE_SECTION("layout");
  this->layoutNode(this->rootNode_, state);
  assert(this->projectionParentScopes_.activeDepth() == 0 &&
         "a macOS projection pass must restore the root scope");
  this->rectSurfaceExtentLedger_.flush();
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

MacScenePlatformController::LayoutNodeResult
MacScenePlatformController::layoutRectSurfaceNode(loka::app::RectSurfaceNode *surface, const LayoutState &state)
{
  loka::app::scene::LayoutState projectedState;
  if (!this->narrowLayoutState(state, projectedState, true))
  {
    return LayoutNodeResult(state.width, state.y);
  }
  const int width = surface->props.width_ > 0 ? surface->props.width_ : state.width;
  const int height = surface->props.height_ > 0 ? surface->props.height_ : state.height;
  this->rectSurfaceExtentLedger_.record(
      surface, loka::core::Frame(state.x, state.y, width, height));
  MacRectSurfaceContext *ctx = static_cast<MacRectSurfaceContext *>(surface->getContext());
  if (ctx)
  {
    ctx->relayout(projectedState.x,
                  projectedState.y,
                  width,
                  height);
  }
  else
  {
    ctx = new MacRectSurfaceContext(
        this,
        this->projectionParentView(),
        projectedState.x,
        projectedState.y,
        width,
        height,
        surface);
    if (!ctx)
    {
      return LayoutNodeResult(state.width, state.y);
    }
    surface->setContext(ctx);
    ctx->readLifecycleFactOnAttach();
  }
  return LayoutNodeResult(
      state.width, state.y + height + loka::app::layout::FallbackControlMetrics::kVerticalSpacing);
}

MacScenePlatformController::LayoutNodeResult
MacScenePlatformController::layoutScrollViewNode(
    loka::app::ScrollViewNode *scrollView,
    const LayoutState &state)
{
  if (!scrollView || this->projectionParentScopes_.activeDepth() != 0)
  {
    // V1 admits one active viewport. Refuse before creating the inner native
    // view so the outer document remains the only structural clipping parent.
    return LayoutNodeResult(state.width, state.y);
  }
  if (state.x < SHRT_MIN || state.x > SHRT_MAX ||
      state.y < SHRT_MIN || state.y > SHRT_MAX ||
      state.width < 0 || state.width > SHRT_MAX ||
      state.height < 0 || state.height > SHRT_MAX ||
      (state.height > 0 && state.y > SHRT_MAX - state.height))
  {
    // The shared traversal above consumes a short result. Refuse the viewport
    // itself before its bottom or any child coordinate would narrow.
    return LayoutNodeResult(state.width, state.y);
  }

  MacScrollViewContext *ctx =
      static_cast<MacScrollViewContext *>(scrollView->getContext());
  if (ctx)
  {
    ctx->relayout(state.x, state.y, state.width, state.height);
  }
  else
  {
    ctx = new MacScrollViewContext(this,
                                   this->projectionParentView(),
                                   state.x,
                                   state.y,
                                   state.width,
                                   state.height,
                                   scrollView);
    if (!ctx || !ctx->isValid())
    {
      delete ctx;
      return LayoutNodeResult(state.width, state.y);
    }
    scrollView->setContext(ctx);
    ctx->readLifecycleFactOnAttach();
  }

  int requestedOffset = scrollView->props.offset_.isValid()
                            ? scrollView->props.offset_.get()
                            : 0;
  if (requestedOffset < 0)
  {
    requestedOffset = 0;
  }
  else if (requestedOffset > SHRT_MAX)
  {
    // A far-out fact must not poison every child coordinate before the exact
    // measured maximum can be republished after the scope has popped.
    requestedOffset = SHRT_MAX;
  }

  const loka::core::Frame viewportClip(0, 0, state.width, state.height);
  loka::app::scene::ProjectionParentScope childScope;
  const loka::app::scene::ProjectionParentScope &parentScope =
      this->projectionParentScopes_.current();
  // AppKit applies scrolling through the clip view's bounds origin. Children
  // therefore subtract the seat origin only; applying requestedOffset here
  // would move them twice.
  if (!parentScope.deriveScrolled(ctx->documentView(),
                                  state.x,
                                  state.y,
                                  viewportClip,
                                  childScope))
  {
    return LayoutNodeResult(state.width, state.y);
  }

  LayoutState childBase = state;
  childBase.width = ctx->contentWidth();
  int currentY = state.y;
  int contentHeight = 0;
  bool shortRangeRefused = false;
  {
    loka::app::scene::ProjectionParentScopeGuard scopeGuard(
        this->projectionParentScopes_, childScope);
    if (!scopeGuard.isActive())
    {
      return LayoutNodeResult(state.width, state.y);
    }

    loka::app::scene::INestable *nestable = scrollView->asNestable();
    loka::dsl::CompositionCursor<loka::app::scene::Node> it(
        nestable ? nestable->childrenHead() : 0,
        nestable ? nestable->childrenCount() : 0);
    for (loka::app::scene::Node *child = it.next(); child; child = it.next())
    {
      if (currentY < SHRT_MIN || currentY > SHRT_MAX)
      {
        this->refuseScrollViewShortRange();
        break;
      }
      LayoutState childState = childBase;
      childState.y = currentY;
      const int nextY = this->layoutNode(child, childState);
      if (this->projectionParentScopes_.current().hasShortRangeRefusal())
      {
        break;
      }
      if (!this->projectionParentScopes_.current().tryAccumulateContentHeight(
              currentY, nextY))
      {
        this->refuseScrollViewShortRange();
        break;
      }
      currentY = nextY;
    }
    contentHeight = this->projectionParentScopes_.current().contentHeight();
    shortRangeRefused =
        this->projectionParentScopes_.current().hasShortRangeRefusal();
  }

  if (!shortRangeRefused)
  {
    const int clampedOffset =
        ctx->setScrollMetrics(contentHeight, state.height, requestedOffset);
    if (scrollView->props.offset_.isValid() &&
        scrollView->props.offset_.get() != clampedOffset)
    {
      // A fact write may schedule another layout. Publish only after the
      // document scope has popped so every projection pass starts at root.
      scrollView->props.offset_.set(clampedOffset);
    }
  }

  if (state.height > 0)
  {
    return LayoutNodeResult(state.width, state.y + state.height);
  }
  if (currentY < SHRT_MIN || currentY > SHRT_MAX)
  {
    return LayoutNodeResult(state.width, state.y);
  }
  return LayoutNodeResult(state.width, currentY);
}

int MacScenePlatformController::layoutNode(loka::app::scene::Node *node, const LayoutState &state)
{
  if (!node)
  {
    return state.y;
  }
  if (this->projectionParentScopes_.activeDepth() != 0 &&
      this->projectionParentScopes_.current().hasShortRangeRefusal())
  {
    return state.y;
  }
  return this->applyBoundaryLayoutResult(node->asBoundary(), state.x, state.y, this->computeLayoutResult(node, state));
}

MacScenePlatformController::LayoutNodeResult
MacScenePlatformController::computeLayoutResult(loka::app::scene::Node *node, const LayoutState &state)
{
  if (loka::app::ScrollViewNode *scrollView = node->asScrollViewNode())
  {
    return this->layoutScrollViewNode(scrollView, state);
  }
  if (loka::app::StackNode *stack = node->asStackNode())
  {
    int resultY = state.y;
    loka::app::scene::IPlatformLayoutHandler *handler = this->layoutHandlerRegistry_.find(stack);
    if (handler)
    {
      loka::app::scene::LayoutState handlerState;
      if (!this->narrowLayoutState(state, handlerState, false))
      {
        return LayoutNodeResult(state.width, state.y);
      }
      loka::app::scene::MacPlatformLayoutTraversal traversal(this);
      resultY = handler->layoutNode(stack, handlerState, &traversal);
    }
    else if (stack->props.axis_ == loka::app::STACK_AXIS_COLUMN)
    {
      resultY = loka::app::layout::computeColumnLayoutResultY(
          stack, state, this, &MacScenePlatformController::layoutContainerChild);
    }
    else
    {
      const loka::app::layout::RowLayoutMetrics metrics =
          loka::app::layout::FallbackControlMetrics::rowLayout();
      resultY = loka::app::layout::computeRowLayoutResultY(
          stack, state, metrics, this, &MacScenePlatformController::layoutContainerChild);
    }
    return LayoutNodeResult(state.width, resultY);
  }

  if (loka::app::GridNode *grid = node->asGridNode())
  {
    int maxY = state.y;
    loka::app::scene::IPlatformLayoutHandler *handler = this->layoutHandlerRegistry_.find(grid);
    if (handler)
    {
      loka::app::scene::LayoutState handlerState;
      if (!this->narrowLayoutState(state, handlerState, false))
      {
        return LayoutNodeResult(state.width, state.y);
      }
      loka::app::scene::MacPlatformLayoutTraversal traversal(this);
      maxY = handler->layoutNode(grid, handlerState, &traversal);
    }
    else
    {
      loka::app::layout::GridLayoutMetrics metrics;
      metrics.gapX = 0;
      metrics.gapY = 0;
      maxY = loka::app::layout::computeGridLayoutResultY(
          grid, state, metrics, this, &MacScenePlatformController::layoutContainerChild);
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
      if (!this->narrowLayoutState(state, handlerState, false))
      {
        return LayoutNodeResult(state.width, state.y);
      }
      loka::app::scene::MacPlatformLayoutTraversal traversal(this);
      resultY = handler->layoutNode(box, handlerState, &traversal);
    }
    else
    {
      resultY = loka::app::layout::computeBoxLayoutResultY(
          box, state, this, &MacScenePlatformController::layoutContainerChild);
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
      if (!this->narrowLayoutState(state, handlerState, false))
      {
        return LayoutNodeResult(state.width, state.y);
      }
      loka::app::scene::MacPlatformLayoutTraversal traversal(this);
      maxY = handler->layoutNode(stack, handlerState, &traversal);
    }
    else
    {
      maxY = loka::app::layout::computeZStackLayoutResultY(
          stack, state, this, &MacScenePlatformController::layoutContainerChild);
    }
    return LayoutNodeResult(state.width, maxY);
  }

  if (loka::app::scene::INestable *nestable = node->asNestable())
  {
    LayoutState childState = state;
    loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
    for (loka::app::scene::Node *child = it.next(); child; child = it.next())
    {
      const int nextY = this->layoutNode(child, childState);
      if (this->refuseNarrowingInScrollScope(nextY))
      {
        break;
      }
      childState.y = nextY;
    }
    return LayoutNodeResult(state.width, childState.y);
  }

  if (node->asProjectedLayoutNode())
  {
    return DispatchProjectedLayout(this, node, state);
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

MacScenePlatformController::LayoutNodeResult
MacScenePlatformController::DispatchProjectedLayout(
    MacScenePlatformController *controller,
    loka::app::scene::Node *node,
    const LayoutState &state)
{
  if (!controller || !node)
  {
    return LayoutNodeResult(state.width, state.y);
  }
  loka::app::scene::IProjectedLayoutNode *projected =
      node->asProjectedLayoutNode();
  if (!projected)
  {
    return LayoutNodeResult(state.width, state.y);
  }
  loka::app::scene::LayoutState projectedState;
  if (!controller->narrowLayoutState(state, projectedState, true))
  {
    return LayoutNodeResult(state.width, state.y);
  }
  const int projectedResult =
      projected->layoutProjected(controller, projectedState);
  if (controller->projectionParentScopes_.activeDepth() == 0)
  {
    return LayoutNodeResult(state.width, projectedResult);
  }
  int contentResult = state.y;
  if (!controller->projectionParentScopes_.current().restoreContentY(
          projectedResult, contentResult))
  {
    controller->refuseScrollViewShortRange();
    return LayoutNodeResult(state.width, state.y);
  }
  return LayoutNodeResult(state.width, contentResult);
}

bool MacScenePlatformController::narrowLayoutState(
    const LayoutState &state,
    loka::app::scene::LayoutState &narrowed,
    bool applyProjection)
{
  if (state.x < SHRT_MIN || state.x > SHRT_MAX ||
      state.y < SHRT_MIN || state.y > SHRT_MAX ||
      state.width < SHRT_MIN || state.width > SHRT_MAX ||
      state.height < SHRT_MIN || state.height > SHRT_MAX)
  {
    this->refuseScrollViewShortRange();
    return false;
  }

  loka::app::scene::LayoutState content;
  content.x = static_cast<short>(state.x);
  content.y = static_cast<short>(state.y);
  content.width = static_cast<short>(state.width);
  content.height = static_cast<short>(state.height);
  content.lineHeight = 0;
  content.spacing = 0;
  if (applyProjection && this->projectionParentScopes_.activeDepth() != 0)
  {
    if (!this->projectionParentScopes_.current().project(content, narrowed))
    {
      this->refuseScrollViewShortRange();
      return false;
    }
    narrowed.lineHeight = content.lineHeight;
    narrowed.spacing = content.spacing;
    return true;
  }
  narrowed = content;
  return true;
}

void MacScenePlatformController::refuseScrollViewShortRange()
{
  if (this->projectionParentScopes_.activeDepth() != 0)
  {
    this->projectionParentScopes_.current().markShortRangeRefused();
  }
}

bool MacScenePlatformController::refuseNarrowingInScrollScope(int resultY)
{
  if (this->projectionParentScopes_.activeDepth() == 0)
  {
    return false;
  }
  loka::app::scene::ProjectionParentScope &scope =
      this->projectionParentScopes_.current();
  if (scope.hasShortRangeRefusal())
  {
    return true;
  }
  if (resultY >= SHRT_MIN && resultY <= SHRT_MAX)
  {
    return false;
  }
  scope.markShortRangeRefused();
  return true;
}

int MacScenePlatformController::layoutContainerChild(void *context,
                                                     loka::app::scene::Node *child,
                                                     const LayoutState &state)
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
    const bool controlTagMatches =
        focusedEditTextControlTag_ != 0 && edit->props.controlTag_ == focusedEditTextControlTag_;
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
  // Parked retained branches (Conditional slots) hand their native pairs
  // over here too — the retire door, not the reclaim drain.
  for (unsigned i = 0; loka::app::scene::Node *branch = node->retainedLifecycleBranch(i); ++i)
  {
    clearNodeContexts(branch);
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
