#include "ToolboxScenePlatformController.hpp"
#include "ToolboxLayoutMetrics.hpp"
#include "ToolboxBuiltInSupport.hpp"
#include "ToolboxNodeDispatch.hpp"
#include "ToolboxPlatformLayoutHandlers.hpp"
#include "ToolboxScrollViewDecisions.hpp"
#include "ToolboxWindow.hpp"
#include "core/Profiler.hpp"
#include <Quickdraw.h>
#include <Controls.h>
#include <cstring>
#include <cassert>
#include <cstdio>
#include <string>
#include <ctime>
#include <climits>
#include <Memory.h>
#include <Menus.h>

#include "platform/StringUTF8.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "core/String.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/Cell.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/nodes/controls/PopupMenu.hpp"
#include "app/nodes/controls/ScrollBar.hpp"
#include "app/nodes/ImageView.hpp"
#include "app/RectSurface.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/nestable/Grid.hpp"
#include "app/nodes/nestable/ZStack.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/ScrollView.hpp"
#include "app/layout/LayoutHeuristics.hpp"
#include "context/ToolboxProjectedNodeContext.hpp"
#include "context/ToolboxPopupMenuContext.hpp"
#include "context/ToolboxButtonContext.hpp"
#include "context/ToolboxCellContext.hpp"
#include "context/ToolboxEditTextContext.hpp"
#include "context/ToolboxTextContext.hpp"
#include "context/ToolboxImageViewContext.hpp"
#include "context/ToolboxRectSurfaceContext.hpp"
#include "context/ToolboxLayoutUtil.hpp"
#include "app/scene/Node.hpp"
#include "app/scene/boundary/Boundary.hpp"

namespace
{
#if !defined(pushButProc) && !defined(LOKA_TOOLBOX_MULTIVERSAL_INTERFACES)
  enum
  {
    pushButProc = 0
  };
#endif

  static const short kAutoControlBaseId = 128;

  void DrawStringAt(short x, short y, const loka::core::String &value)
  {
    std::string utf8;
    if (!loka::platform::CollectUtf8(value, utf8))
    {
      return;
    }
    std::size_t length = utf8.size();
    if (length > 255)
    {
      length = 255;
    }
    Str255 text;
    text[0] = static_cast<unsigned char>(length);
    if (length > 0)
    {
      std::memcpy(text + 1, utf8.data(), length);
    }
    MoveTo(x, y);
    DrawString(text);
  }


  void CopyToPascalString(const loka::core::String &value, Str255 out)
  {
    std::string utf8;
    if (!loka::platform::CollectUtf8(value, utf8))
    {
      out[0] = 0;
      return;
    }
    std::size_t length = utf8.size();
    if (length > 255)
    {
      length = 255;
    }
    out[0] = static_cast<unsigned char>(length);
    if (length > 0)
    {
      std::memcpy(out + 1, utf8.data(), length);
    }
  }

  bool UseBoundaryDirty(const loka::app::scene::BoundaryNode *boundary)
  {
    return boundary && boundary->parentBoundary() && boundary->hasLayoutBounds();
  }

  Rect BoundaryToRect(const loka::app::scene::BoundaryNode *boundary, const Rect &fallback)
  {
    if (!UseBoundaryDirty(boundary))
    {
      return fallback;
    }
    const loka::app::scene::BoundaryNode::LayoutBounds &bounds = boundary->layoutBounds();
    Rect rect;
    rect.left = static_cast<short>(bounds.x);
    rect.top = static_cast<short>(bounds.y);
    rect.right = static_cast<short>(bounds.x + bounds.width);
    rect.bottom = static_cast<short>(bounds.y + bounds.height);
    return rect;
  }

  short MaxExplicitControlId(loka::app::scene::Node *node)
  {
    if (!node)
    {
      return 0;
    }
    short maxId = 0;
    if (loka::app::ButtonNode *button = node->asButtonNode())
    {
      short id = 0;
      if (button->props.controlTag_ > 0 && button->props.controlTag_ <= 32767)
      {
        id = static_cast<short>(button->props.controlTag_);
      }
      if (id > maxId)
      {
        maxId = id;
      }
    }
    if (loka::app::EditTextNode *edit = node->asEditTextNode())
    {
      short id = 0;
      if (edit->props.controlTag_ > 0 && edit->props.controlTag_ <= 32767)
      {
        id = static_cast<short>(edit->props.controlTag_);
      }
      if (id > maxId)
      {
        maxId = id;
      }
    }
    if (loka::app::PopupMenuNode *popup = node->asPopupMenuNode())
    {
      short id = 0;
      if (popup->props.controlTag_ > 0 && popup->props.controlTag_ <= 32767)
      {
        id = static_cast<short>(popup->props.controlTag_);
      }
      if (id > maxId)
      {
        maxId = id;
      }
    }
    if (loka::app::ScrollBarNode *scrollBar = node->asScrollBarNode())
    {
      short id = 0;
      if (scrollBar->props.controlTag_ > 0 && scrollBar->props.controlTag_ <= 32767)
      {
        id = static_cast<short>(scrollBar->props.controlTag_);
      }
      if (id > maxId)
      {
        maxId = id;
      }
    }
    if (loka::app::scene::INestable *nestable = node->asNestable())
    {
      loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
      for (loka::app::scene::Node *child = it.next(); child; child = it.next())
      {
        short childMax = MaxExplicitControlId(child);
        if (childMax > maxId)
        {
          maxId = childMax;
        }
      }
    }
    return maxId;
  }

  bool HasRectSurfaceNode(loka::app::scene::Node *node)
  {
    if (!node)
    {
      return false;
    }
    if (node->kind() == loka::app::scene::NODE_KIND_RECT_SURFACE)
    {
      return true;
    }
    if (loka::app::scene::INestable *nestable = node->asNestable())
    {
      loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
      for (loka::app::scene::Node *child = it.next(); child; child = it.next())
      {
        if (HasRectSurfaceNode(child))
        {
          return true;
        }
      }
    }
    return false;
  }

  bool HasZStackNode(loka::app::scene::Node *node)
  {
    if (!node)
    {
      return false;
    }
    if (node->kind() == loka::app::scene::NODE_KIND_ZSTACK)
    {
      return true;
    }
    if (loka::app::scene::INestable *nestable = node->asNestable())
    {
      loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
      for (loka::app::scene::Node *child = it.next(); child; child = it.next())
      {
        if (HasZStackNode(child))
        {
          return true;
        }
      }
    }
    return false;
  }

  loka::app::RectSurfacePaintResult RenderDirtyRectSurfaces(loka::app::scene::Node *node,
                                                            const Rect &dirtyRect)
  {
    if (!node)
    {
      return loka::app::RECT_SURFACE_PAINT_SUCCEEDED;
    }
    if (loka::app::RectSurfaceNode *surface = node->asRectSurfaceNode())
    {
      ToolboxRectSurfaceContext *ctx = static_cast<ToolboxRectSurfaceContext *>(surface->getContext());
      if (ctx)
      {
        return ctx->renderDirty(dirtyRect);
      }
      return loka::app::RECT_SURFACE_PAINT_SUCCEEDED;
    }
    loka::app::RectSurfacePaintResult result = loka::app::RECT_SURFACE_PAINT_SUCCEEDED;
    if (loka::app::scene::INestable *nestable = node->asNestable())
    {
      loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
      for (loka::app::scene::Node *child = it.next(); child; child = it.next())
      {
        if (RenderDirtyRectSurfaces(child, dirtyRect) == loka::app::RECT_SURFACE_PAINT_REFUSED)
        {
          result = loka::app::RECT_SURFACE_PAINT_REFUSED;
        }
      }
    }
    return result;
  }

  bool CollectRectSurfaceDirtyRect(loka::app::scene::Node *node, Rect &outRect)
  {
    if (!node)
    {
      return false;
    }
    bool hasRect = false;
    if (loka::app::RectSurfaceNode *surface = node->asRectSurfaceNode())
    {
      ToolboxRectSurfaceContext *ctx = static_cast<ToolboxRectSurfaceContext *>(surface->getContext());
      if (ctx)
      {
        Rect rect;
        if (ctx->dirtyRect(rect))
        {
          outRect = rect;
          return true;
        }
      }
    }
    if (loka::app::scene::INestable *nestable = node->asNestable())
    {
      loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
      for (loka::app::scene::Node *child = it.next(); child; child = it.next())
      {
        Rect childRect;
        if (!CollectRectSurfaceDirtyRect(child, childRect))
        {
          continue;
        }
        if (!hasRect)
        {
          outRect = childRect;
          hasRect = true;
        }
        else
        {
          if (childRect.left < outRect.left)
          {
            outRect.left = childRect.left;
          }
          if (childRect.top < outRect.top)
          {
            outRect.top = childRect.top;
          }
          if (childRect.right > outRect.right)
          {
            outRect.right = childRect.right;
          }
          if (childRect.bottom > outRect.bottom)
          {
            outRect.bottom = childRect.bottom;
          }
        }
      }
    }
    return hasRect;
  }

  bool ContainsOnlyRectSurfacePainting(loka::app::scene::Node *node)
  {
    if (!node)
    {
      return false;
    }
    if (node->asRectSurfaceNode())
    {
      return true;
    }
    loka::app::scene::INestable *nestable = node->asNestable();
    if (!nestable)
    {
      return false;
    }
    bool hasChild = false;
    loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
    for (loka::app::scene::Node *child = it.next(); child; child = it.next())
    {
      hasChild = true;
      if (!ContainsOnlyRectSurfacePainting(child))
      {
        return false;
      }
    }
    return hasChild;
  }



  bool RectsIntersect(const Rect &a, const Rect &b)
  {
    if (a.right < b.left || a.left > b.right)
    {
      return false;
    }
    if (a.bottom < b.top || a.top > b.bottom)
    {
      return false;
    }
    return true;
  }
} // namespace

// Hard per-bucket bound so a one-off control spike cannot pin its high-water
// mark of native handles for the window's whole lifetime (4MB-class targets).
// Provisional policy: revisit with measured hit/miss/evict counters.
static const std::size_t kNativePoolBucketDepthCap = 8;

ToolboxScenePlatformController::ToolboxScenePlatformController(ToolboxWindow *window)
    : window_(window),
      projectionParentScopes_(window && window->window()
                                  ? static_cast<void *>(window->window())
                                  : 0),
      rootNode_(0),
      pendingRootNode_(0),
      scrollBarLedger_(kNativePoolBucketDepthCap),
      focusedText_(0),
      focusedRect_(),
      hasFocusedRect_(false),
      enabledStateBindingPath_(this),
      inBatchUpdate_(false),
      pendingFullInvalidate_(false),
      pendingInvalidateFlags_(loka::app::scene::NODE_DIRTY_NONE),
      forceFullRedraw_(false),
      pendingDirtyRects_(),
      retiredControls_(),
      retiredScrollBarControls_(),
      retiredTextEdits_(),
      pushButtonBucket_(kNativePoolBucketDepthCap),
      textEditBucket_(kNativePoolBucketDepthCap),
      poolIntakeAuditFailCount_(0),
      clipRgn_(NewRgn()),
      scrollViewClipRgn_(NewRgn()),
      hasClip_(false),
      controlIds_(kAutoControlBaseId),
      debugStats_(),
      activeLayoutBoundary_(0)
{
  RegisterToolboxPlatformLayoutHandlers(this->layoutHandlerRegistry_);
  const bool builtInsRegistered = RegisterToolboxBuiltInSupport(*this);
  (void)builtInsRegistered;
  // A refused HandlerEntry allocation would leave that node kind silently
  // unprojectable for this controller's lifetime; surface it at boot.
  assert(builtInsRegistered && "Toolbox built-in node handler registration failed at boot");
}

ToolboxScenePlatformController::~ToolboxScenePlatformController()
{
  clearTextBindings();
  clearControls();
  flushRetiredNativeHandles();
  drainNativeHandleBuckets();
  if (clipRgn_)
  {
    DisposeRgn(clipRgn_);
    clipRgn_ = 0;
  }
  if (scrollViewClipRgn_)
  {
    DisposeRgn(scrollViewClipRgn_);
    scrollViewClipRgn_ = 0;
  }
}

bool ToolboxScenePlatformController::registerNodeHandler(loka::app::scene::IPlatformNodeHandler *handler)
{
  return this->nodeHandlerRegistry_.registerHandler(handler);
}

bool ToolboxScenePlatformController::prepareProjectedLayout(loka::app::scene::Node *node,
                                                            loka::app::scene::LayoutState &state)
{
  if (!node)
  {
    return false;
  }
  if (!this->projectLayoutState(state))
  {
    return false;
  }
  loka::app::scene::IPlatformNodeHandler *handler = this->nodeHandlerRegistry_.find(node);
  if (!handler)
  {
    assert(false && "no node handler registered for this node type -- register the handler or an explicit RefusedNodeHandler");
    return false;
  }
  loka::app::scene::NodeContext *previousContext = node->getContext();
  loka::app::scene::NodeContext *context = handler->ensureContext(node, this, state);
  if (!context)
  {
    return false;
  }
  if (context != previousContext)
  {
    this->requestStructurePresent();
  }
  // Type-safe hookup: only contexts that opt in through asBoundaryTagged
  // receive the tag, so a foreign handler returning a plain NodeContext (the
  // registry is public API) can never be written through a wrong downcast.
  // The OpenFileDialog context simply does not opt in.
  if (loka::app::scene::IBoundaryTaggedContext *tagged = context->asBoundaryTagged())
  {
    tagged->setBoundary(this->activeLayoutBoundary());
  }
  return true;
}

void ToolboxScenePlatformController::refuseScrollViewShortRange()
{
  if (this->projectionParentScopes_.activeDepth() != 0)
  {
    this->projectionParentScopes_.current().markShortRangeRefused();
  }
}

bool ToolboxScenePlatformController::refuseNarrowingInScrollScope(int resultY)
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

short ToolboxScenePlatformController::layoutScrollView(
    loka::app::ScrollViewNode *scrollView,
    loka::app::scene::LayoutState &state,
    loka::app::scene::BoundaryNode *currentBoundary)
{
  if (this->projectionParentScopes_.activeDepth() != 0)
  {
    // V1 admits one viewport. The inner subtree materializes nothing, while
    // the outer measurement scope remains usable for following siblings.
    return 0;
  }
  if (!this->scrollViewClipRgn_)
  {
    // NewRgn can refuse under Classic low memory. Without the clip region the
    // render pass cannot draw the subtree, so refuse the whole seat here -
    // before any ledger, CDEF, or hit registration - instead of leaving a
    // scrollbar over invisible interactive content.
    return 0;
  }

  const int viewportRight = static_cast<int>(state.x) + state.width;
  const int viewportBottom = static_cast<int>(state.y) + state.height;
  if (state.width < 0 || state.height < 0 ||
      viewportRight > SHRT_MAX || viewportBottom > SHRT_MAX)
  {
    // Rect and LayoutState edges are both short on Toolbox. Refuse the seat
    // before any child context or CDEF is installed from a narrowed rect.
    return 0;
  }

  Rect viewportRect;
  viewportRect.left = state.x;
  viewportRect.top = state.y;
  viewportRect.right = static_cast<short>(viewportRight);
  viewportRect.bottom = static_cast<short>(viewportBottom);

  const int requestedFact = scrollView->props.offset_.isValid()
                                ? scrollView->props.offset_.get()
                                : 0;
  int projectedOffset = requestedFact;
  if (projectedOffset < 0)
  {
    projectedOffset = 0;
  }
  else if (projectedOffset > SHRT_MAX)
  {
    // Pre-clamp only for safe projection. The exact measured maximum is
    // published after the scope has popped below.
    projectedOffset = SHRT_MAX;
  }

  const loka::core::Frame viewportClip(
      state.x, state.y, state.width, state.height);
  loka::app::scene::ProjectionParentScope childScope;
  const loka::app::scene::ProjectionParentScope &parentScope =
      this->projectionParentScopes_.current();
  if (!parentScope.deriveScrolled(
          parentScope.nativeParent, 0, projectedOffset,
          viewportClip, childScope))
  {
    return 0;
  }

  const short seatY = state.y;
  int contentHeight = 0;
  bool shortRangeRefused = false;
  {
    loka::app::scene::ProjectionParentScopeGuard scopeGuard(
        this->projectionParentScopes_, childScope);
    if (!scopeGuard.isActive())
    {
      return 0;
    }

    loka::app::scene::LayoutState childState = state;
    childState.width = static_cast<short>(
        ToolboxScrollViewChildWidth(state.width));
    loka::dsl::CompositionCursor<loka::app::scene::Node> it(
        scrollView->childrenHead(), scrollView->childrenCount());
    for (loka::app::scene::Node *child = it.next(); child; child = it.next())
    {
      const int childStartY = childState.y;
      LayoutNode(child, childState, this, currentBoundary);
      if (this->projectionParentScopes_.current().hasShortRangeRefusal())
      {
        break;
      }
      const int nextY = childState.y;
      if (!this->projectionParentScopes_.current().tryAccumulateContentHeight(
              childStartY, nextY))
      {
        this->refuseScrollViewShortRange();
        break;
      }
    }
    contentHeight = this->projectionParentScopes_.current().contentHeight();
    shortRangeRefused =
        this->projectionParentScopes_.current().hasShortRangeRefusal();
  }

  if (!shortRangeRefused)
  {
    const int clampedOffset = this->ensureViewportScrollBarControl(
        viewportRect, scrollView, contentHeight,
        state.height, requestedFact);
    if (scrollView->props.offset_.isValid() &&
        ToolboxScrollViewShouldRepublish(
            scrollView->props.offset_.get(), clampedOffset))
    {
      // The NodeState door opens the ScrollView owner's tracker when idle.
      // Publish only after the projection-parent scope has popped.
      scrollView->props.offset_.set(clampedOffset);
    }
  }

  if (state.height > 0)
  {
    state.y = static_cast<short>(viewportBottom);
  }
  else if (!shortRangeRefused &&
           static_cast<int>(seatY) + contentHeight <= SHRT_MAX)
  {
    // The channel caps contentHeight at SHRT_MAX, but the seat origin rides
    // on top of it: the measured bottom must also fit the short before it
    // narrows (the same wrap the Win32 arm refuses on its heightless path).
    state.y = static_cast<short>(seatY + contentHeight);
  }
  else
  {
    if (!shortRangeRefused)
    {
      this->refuseScrollViewShortRange();
    }
    state.y = seatY;
  }
  return state.width;
}

void ToolboxScenePlatformController::renderScrollView(
    loka::app::ScrollViewNode *scrollView)
{
  const ViewportScrollBarBinding *binding = 0;
  for (std::size_t i = 0; i < this->scrollBarLedger_.viewportScrollBars_.size(); ++i)
  {
    if (this->scrollBarLedger_.viewportScrollBars_[i].scrollView == scrollView)
    {
      binding = &this->scrollBarLedger_.viewportScrollBars_[i];
      break;
    }
  }
  if (!binding || !binding->usedThisFrame ||
      !this->scrollViewClipRgn_ ||
      this->projectionParentScopes_.activeDepth() != 0)
  {
    return;
  }
  const Rect viewportRect = binding->rect;
  const loka::core::Frame viewportClip(
      viewportRect.left,
      viewportRect.top,
      viewportRect.right - viewportRect.left,
      viewportRect.bottom - viewportRect.top);
  loka::app::scene::ProjectionParentScope renderScope(
      this->projectionParentScopes_.current().nativeParent,
      0,
      0,
      viewportClip);

  GetClip(this->scrollViewClipRgn_);
  ClipRect(&viewportRect);
  {
    loka::app::scene::ProjectionParentScopeGuard scopeGuard(
        this->projectionParentScopes_, renderScope);
    if (scopeGuard.isActive())
    {
      RenderChildren(scrollView, this);
    }
  }
  SetClip(this->scrollViewClipRgn_);
}

short ToolboxScenePlatformController::allocateControlId()
{
  return controlIds_.allocate();
}

void ToolboxScenePlatformController::onChange(loka::app::scene::Node *rootNode,
                                              loka::app::scene::NodeDirtyFlags flags,
                                              bool fullRebuild)
{
  rootNode_ = rootNode;
  debugStats_.begin(flags, fullRebuild);
  debugStats_.lastRootPresent = (rootNode != 0);
  if (!window_ || !window_->window())
  {
    return;
  }
  if (inBatchUpdate_)
  {
    ++debugStats_.batchOnChangeCount;
    ++debugStats_.batchAccumOnChangeCount;
    if (!debugStats_.batchAccumTrace.empty())
    {
      debugStats_.batchAccumTrace += " ";
    }
    debugStats_.batchAccumTrace += ToolboxSceneDebugStats::flagsToString(flags);
    debugStats_.batchAccumTrace += "/";
    debugStats_.batchAccumTrace += fullRebuild ? "1" : "0";
    debugStats_.batchAccumTrace += "/";
    debugStats_.batchAccumTrace += rootNode ? "1" : "0";
    if (!rootNode)
    {
      ++debugStats_.batchNullRootCount;
      ++debugStats_.batchAccumNullRootCount;
    }
    if (fullRebuild)
    {
      ++debugStats_.batchFullRebuildCount;
      debugStats_.batchAccumFullRebuild = true;
    }
    if (flags != loka::app::scene::NODE_DIRTY_NONE)
    {
      ++debugStats_.batchNonNoneFlagsCount;
    }
    debugStats_.batchAccumFlags = static_cast<loka::app::scene::NodeDirtyFlags>(debugStats_.batchAccumFlags | flags);
    pendingInvalidateFlags_ = static_cast<loka::app::scene::NodeDirtyFlags>(pendingInvalidateFlags_ | flags);
    if (rootNode)
    {
      pendingRootNode_ = rootNode;
    }
    if (fullRebuild)
    {
      pendingFullInvalidate_ = true;
    }
    return;
  }
  requestInvalidateForChange(rootNode, flags, fullRebuild);
}

void ToolboxScenePlatformController::onBoundaryApply(loka::app::scene::Node *rootNode,
                                                     loka::app::scene::BoundaryNode *boundary,
                                                     const loka::app::scene::BoundaryLocalApplyInfo &info,
                                                     const loka::app::scene::PlatformApplyPlan &plan)
{
  if (rootNode)
  {
    rootNode_ = rootNode;
  }
  if (!window_ || !window_->window() || !rootNode_ || !boundary || !plan.hasBoundaryApplyWork(boundary))
  {
    return;
  }
  if (!info.hasAnyWork())
  {
    return;
  }

  if (!this->scrollBarLedger_.viewportScrollBars_.empty())
  {
    // Boundary bounds are recorded in content coordinates; inside a scrolled
    // viewport they no longer name window pixels, and a rect invalidation
    // would leave the OS update region clipping the redraw to the wrong
    // place. Escalate to a full-window invalidation whenever a viewport is
    // installed - the same conservative fallback renderDirty already takes:
    // overpaint, never stale pixels. Projecting per-boundary bounds through
    // the scope is the recorded alternative for a later pass (#518's
    // projection-target track).
    window_->requestInvalidate();
    return;
  }

  if (!info.hasBoundsHint())
  {
    Rect surfaceDirtyRect;
    if (info.hasPaintWork() && ContainsOnlyRectSurfacePainting(boundary)
        && CollectRectSurfaceDirtyRect(boundary, surfaceDirtyRect))
    {
      window_->requestInvalidateRect(surfaceDirtyRect);
      return;
    }
    loka::app::scene::Node *firstChild = 0;
    if (loka::app::scene::INestable *nestable = boundary->asNestable())
    {
      loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
      firstChild = it.next();
    }
    if (boundary->kind() == loka::app::scene::NODE_KIND_UNKNOWN && boundary->testId().empty() && firstChild
        && firstChild->kind() == loka::app::scene::NODE_KIND_ZSTACK)
    {
      return;
    }
    if (boundary->hasLayoutBounds())
    {
      window_->requestInvalidateRect(BoundaryToRect(boundary, window_->window()->portRect));
    }
    else
    {
      window_->requestInvalidate();
    }
    return;
  }

  Rect rect;
  rect.left = static_cast<short>(info.bounds->x);
  rect.top = static_cast<short>(info.bounds->y);
  rect.right = static_cast<short>(info.bounds->x + info.bounds->width);
  rect.bottom = static_cast<short>(info.bounds->y + info.bounds->height);
  Rect surfaceDirtyRect;
  if (CollectRectSurfaceDirtyRect(boundary, surfaceDirtyRect))
  {
    if (surfaceDirtyRect.left < rect.left)
    {
      rect.left = surfaceDirtyRect.left;
    }
    if (surfaceDirtyRect.top < rect.top)
    {
      rect.top = surfaceDirtyRect.top;
    }
    if (surfaceDirtyRect.right > rect.right)
    {
      rect.right = surfaceDirtyRect.right;
    }
    if (surfaceDirtyRect.bottom > rect.bottom)
    {
      rect.bottom = surfaceDirtyRect.bottom;
    }
  }
  window_->requestInvalidateRect(rect);
}

void ToolboxScenePlatformController::synchronize()
{
  // Toolbox doesn't have a retained scene graph; rely on Update events.
}

bool ToolboxScenePlatformController::hasPendingSync() const
{
  return !this->retiredControls_.empty() || !this->retiredScrollBarControls_.empty()
         || !this->retiredTextEdits_.empty();
}

void ToolboxScenePlatformController::drainNativeRetirements()
{
  this->flushRetiredNativeHandles();
}

void ToolboxScenePlatformController::destroy()
{
  rootNode_ = 0;
  hitLedger_.popupHits_.clear();
  clearTextBindings();
  clearEnabledBindings();
  clearControls();
  flushRetiredNativeHandles();
  drainNativeHandleBuckets();
}

void ToolboxScenePlatformController::releaseNodeContexts(loka::app::scene::Node *node)
{
  if (!node)
  {
    return;
  }
  for (unsigned i = 0; loka::app::scene::Node *branch = node->retainedLifecycleBranch(i); ++i)
  {
    this->releaseNodeContexts(branch);
  }
  loka::app::scene::INestable *nestable = node->asNestable();
  if (nestable)
  {
    for (loka::app::scene::Node *child = nestable->childrenHead(); child; child = child->nextInComposition)
    {
      this->releaseNodeContexts(child);
    }
  }

  if (loka::app::ScrollViewNode *scrollView = node->asScrollViewNode())
  {
    // The viewport ledger is not a NodeContext: remove its native event door
    // on the same synchronous detach path before the node can be reclaimed.
    this->destroyViewportScrollBarControl(
        scrollView, node->nativeLifetimeHint());
  }

  if (node->getContext())
  {
    this->requestStructurePresent();
  }
  node->setContext(0);
}

void ToolboxScenePlatformController::requestStructurePresent()
{
  if (window_)
  {
    window_->requestInvalidateWithReason("structure-swap");
  }
}

void ToolboxScenePlatformController::retireNodeContext(loka::app::scene::NodeContext *context,
                                                       loka::app::scene::NativeLifetimeHint lifetimeHint)
{
  if (context)
  {
    std::vector<loka::core::State<loka::core::String> *> retiredTextStates;
    std::vector<loka::core::State<bool> *> retiredEnabledStates;

    this->retireEditTextControl(context, lifetimeHint);
    assert(!editControls_.contains(context) &&
           "detach must strip the context's native edit binding before context reclaim");

    for (size_t i = 0; i < hitLedger_.buttonHits_.size();)
    {
      if (static_cast<loka::app::scene::NodeContext *>(hitLedger_.buttonHits_[i].context) == context)
      {
        retiredEnabledStates.push_back(hitLedger_.buttonHits_[i].enabled);
        hitLedger_.buttonHits_.erase(hitLedger_.buttonHits_.begin() + i);
      }
      else
      {
        ++i;
      }
    }
    for (size_t i = 0; i < hitLedger_.cellHits_.size();)
    {
      if (static_cast<loka::app::scene::NodeContext *>(hitLedger_.cellHits_[i].context) == context)
      {
        retiredTextStates.push_back(hitLedger_.cellHits_[i].text);
        hitLedger_.cellHits_.erase(hitLedger_.cellHits_.begin() + i);
      }
      else
      {
        ++i;
      }
    }
    for (size_t i = 0; i < hitLedger_.popupHits_.size();)
    {
      if (static_cast<loka::app::scene::NodeContext *>(hitLedger_.popupHits_[i].context) == context)
      {
        retiredEnabledStates.push_back(hitLedger_.popupHits_[i].enabled);
        hitLedger_.popupHits_.erase(hitLedger_.popupHits_.begin() + i);
      }
      else
      {
        ++i;
      }
    }

    loka::core::State<loka::core::String> *projectedText = context->projectedTextState();
    if (projectedText)
    {
      retiredTextStates.push_back(projectedText);
      for (size_t i = 0; i < hitLedger_.editHits_.size();)
      {
        if (hitLedger_.editHits_[i].text == projectedText)
        {
          hitLedger_.editHits_.erase(hitLedger_.editHits_.begin() + i);
        }
        else
        {
          ++i;
        }
      }
      for (size_t i = 0; i < hitLedger_.textHits_.size();)
      {
        if (hitLedger_.textHits_[i].text == projectedText)
        {
          hitLedger_.textHits_.erase(hitLedger_.textHits_.begin() + i);
        }
        else
        {
          ++i;
        }
      }
    }

    for (size_t i = 0; i < retiredTextStates.size(); ++i)
    {
      loka::core::State<loka::core::String> *text = retiredTextStates[i];
      if (text && !this->hasLiveBinding(text))
      {
        this->unbindTextState(text);
      }
    }
    for (size_t i = 0; i < retiredEnabledStates.size(); ++i)
    {
      loka::core::State<bool> *enabled = retiredEnabledStates[i];
      if (enabled && !this->hasLiveBinding(enabled))
      {
        this->unbindEnabledState(enabled);
      }
    }
  }
}

void ToolboxScenePlatformController::render()
{
  PROFILE_FUNC();
  ++debugStats_.renderCalls;
  ++debugStats_.totalRenderCalls;
  if (!window_ || !window_->window() || !rootNode_)
  {
    return;
  }
  {
    // Identity rule: an auto id stays with its context for the context's
    // lifetime. Observed explicit tags only ever RAISE the auto range —
    // resetting the counter here made lazily-allocating contexts collide
    // after a Show reveal (issue #120).
    controlIds_.raiseBaseAbove(MaxExplicitControlId(rootNode_));
  }
  {
    hitLedger_.buttonHits_.clear();
    hitLedger_.cellHits_.clear();
    for (size_t i = 0; i < buttonControls_.size(); ++i)
    {
      buttonControls_[i].usedThisFrame = false;
    }
    for (size_t i = 0; i < scrollBarLedger_.scrollBarControls_.size(); ++i)
    {
      scrollBarLedger_.scrollBarControls_[i].usedThisFrame = false;
    }
    for (size_t i = 0; i < scrollBarLedger_.viewportScrollBars_.size(); ++i)
    {
      scrollBarLedger_.viewportScrollBars_[i].usedThisFrame = false;
    }
    hitLedger_.editHits_.clear();
    for (size_t i = 0; i < editControls_.size(); ++i)
    {
      editControls_[i].usedThisFrame = false;
    }
    hitLedger_.textHits_.clear();
    hitLedger_.popupHits_.clear();
    clearEnabledBindings();
    pendingTextStates_.clear();
    pendingDirtyRects_.clear();
  }
  loka::app::scene::LayoutState state;
  state.x = 12;
  state.y = 24;
  state.lineHeight = 14;
  state.spacing = 6;
  {
    Rect port = window_->window()->portRect;
    short width = static_cast<short>(port.right - port.left - state.x * 2);
    short height = static_cast<short>(port.bottom - port.top - state.y * 2);
    if (width < 0)
    {
      width = 0;
    }
    if (height < 0)
    {
      height = 0;
    }
    state.width = width;
    state.height = height;
  }
  assert(this->projectionParentScopes_.activeDepth() == 0 &&
         "a Toolbox projection pass must begin at the root scope");
  const Rect rootPort = window_->window()->portRect;
  const loka::core::Frame rootClip(
      rootPort.left,
      rootPort.top,
      rootPort.right - rootPort.left,
      rootPort.bottom - rootPort.top);
  if (!this->projectionParentScopes_.resetRoot(
          static_cast<void *>(window_->window()), rootClip))
  {
    return;
  }
  PROFILE_SECTION("layout");
  LayoutNode(rootNode_, state, this, 0);
  RenderNode(rootNode_, this);
  assert(this->projectionParentScopes_.activeDepth() == 0 &&
         "a Toolbox projection pass must restore the root scope");
  debugStats_.refreshHitCounts(static_cast<int>(hitLedger_.buttonHits_.size()),
                               static_cast<int>(hitLedger_.cellHits_.size()),
                               static_cast<int>(hitLedger_.editHits_.size()),
                               static_cast<int>(hitLedger_.textHits_.size()),
                               static_cast<int>(hitLedger_.popupHits_.size()));
  {
    for (size_t i = 0; i < buttonControls_.size(); ++i)
    {
      if (!buttonControls_[i].usedThisFrame && buttonControls_[i].control)
      {
        HideControl(buttonControls_[i].control);
      }
    }
    for (size_t i = 0; i < scrollBarLedger_.scrollBarControls_.size(); ++i)
    {
      if (!scrollBarLedger_.scrollBarControls_[i].usedThisFrame && scrollBarLedger_.scrollBarControls_[i].control)
      {
        HideControl(scrollBarLedger_.scrollBarControls_[i].control);
      }
    }
    for (size_t i = 0; i < editControls_.size();)
    {
      if (!editControls_[i].usedThisFrame)
      {
        this->retireEditTextControlAt(i, editControls_[i].lifetimeHint);
        continue;
      }
      ++i;
    }
  }
}

void ToolboxScenePlatformController::renderDirty(const Rect &rect)
{
  ++debugStats_.renderDirtyCalls;
  ++debugStats_.totalRenderDirtyCalls;
  if (!window_ || !window_->window() || !rootNode_)
  {
    return;
  }
  if (forceFullRedraw_)
  {
    forceFullRedraw_ = false;
    render();
    return;
  }
  if (!scrollBarLedger_.viewportScrollBars_.empty())
  {
    // Direct dirty RectSurface replay bypasses the node render switch. Keep
    // the viewport's central clip authoritative whenever one is installed.
    render();
    return;
  }
  if (hitLedger_.textHits_.empty() && hitLedger_.popupHits_.empty() && buttonControls_.empty() && scrollBarLedger_.scrollBarControls_.empty()
      && editControls_.empty())
  {
    if (HasRectSurfaceNode(rootNode_))
    {
      if (RenderDirtyRectSurfaces(rootNode_, rect) == loka::app::RECT_SURFACE_PAINT_REFUSED)
      {
        // ToolboxWindow::flushInvalidate snapshots and clears the current
        // pending-rect batch before calling renderDirty. This request enters
        // the window's next batch, so persistent allocation refusal costs one
        // attempt per later event-loop flush rather than spinning here.
        window_->requestInvalidateRect(rect);
      }
    }
    else
    {
      render();
    }
    return;
  }
  bool dirtyIntersectsText = false;
  for (size_t i = 0; i < hitLedger_.textHits_.size(); ++i)
  {
    if (RectsIntersect(rect, hitLedger_.textHits_[i].rect))
    {
      dirtyIntersectsText = true;
      break;
    }
  }
  if (dirtyIntersectsText && HasZStackNode(rootNode_))
  {
    // A ZStack declares shared pixels. Rebuild the registries only before any
    // replay prefix is frozen (#315), and let the clipped render walk own the
    // dirty pixels instead of letting one text run erase its siblings.
    GrafPtr oldPort;
    GetPort(&oldPort);
    SetPort(window_->window());
    // Own the clip save locally: beginClip/endClip share one region and one
    // flag, and the walk below re-enters them (EditText::draw clips TEUpdate),
    // so nesting through the shared pair would leave the port clipped to this
    // dirty rect after the inner endClip consumed the flag. Same shape as
    // redrawTextHit's save/restore.
    RgnHandle oldClip = NewRgn();
    if (oldClip != 0)
    {
      GetClip(oldClip);
      ClipRect(&rect);
    }
    EraseRect(&rect);
    render();
    if (oldClip != 0)
    {
      SetClip(oldClip);
      DisposeRgn(oldClip);
    }
    drawControlsInRect(rect);
    SetPort(oldPort);
    return;
  }
  if (HasRectSurfaceNode(rootNode_))
  {
    if (RenderDirtyRectSurfaces(rootNode_, rect) == loka::app::RECT_SURFACE_PAINT_REFUSED)
    {
      window_->requestInvalidateRect(rect);
    }
  }
  for (size_t i = 0; i < hitLedger_.popupHits_.size(); ++i)
  {
    PopupHit &hit = hitLedger_.popupHits_[i];
    if (!RectsIntersect(rect, hit.rect))
    {
      continue;
    }
    redrawPopupHit(hit);
  }
  // Replay over a frozen prefix, by value: registration belongs to the render
  // walk (#315), so the registry must not change under this loop. The frozen
  // bound and the copied entry keep a regressed registrar from turning this
  // into an unbounded loop or a dangling reference even where the assert is
  // compiled out; the assert makes the contract loud where it is not.
  const size_t cellReplayCount = hitLedger_.cellHits_.size();
  for (size_t i = 0; i < cellReplayCount; ++i)
  {
    CellHit hit = hitLedger_.cellHits_[i];
    if (!hit.context)
    {
      continue;
    }
    if (!RectsIntersect(rect, hit.rect))
    {
      continue;
    }
    hit.context->draw(this);
    assert(hitLedger_.cellHits_.size() == cellReplayCount
           && "cell hits register on the render walk; the dirty replay must not grow the registry it iterates (#315)");
  }
  for (size_t i = 0; i < hitLedger_.textHits_.size(); ++i)
  {
    TextHit &hit = hitLedger_.textHits_[i];
    if (!RectsIntersect(rect, hit.rect))
    {
      continue;
    }
    redrawTextHit(hit);
  }
  const size_t editReplayCount = editControls_.size();
  for (size_t i = 0; i < editReplayCount; ++i)
  {
    EditTextControlBinding &binding = editControls_[i];
    if (!binding.ownerContext || !binding.te || !binding.usedThisFrame)
    {
      continue;
    }
    // binding.rect is the inset text rect that TEUpdate needs; draw() frames
    // the outer rect, so the region that has to trigger a redraw is the outer
    // one. Gating on the inner rect would skip a dirty strip covering only the
    // chrome and leave the frame erased.
    if (!RectsIntersect(rect, binding.ownerContext->chromeRect()))
    {
      continue;
    }
    // draw() re-enters ensureEditTextControl, which can add to editControls_,
    // so the owner is read out before the call and the bound is a snapshot:
    // the binding reference must not survive a reallocation, and the replay
    // must not iterate entries it created. Same wall as the cell replay above.
    ToolboxEditTextContext *owner = binding.ownerContext;
    owner->draw(this);
    assert(editControls_.size() == editReplayCount
           && "edit controls register on the render walk; the dirty replay must not grow the registry it iterates (#315)");
  }
  drawControlsInRect(rect);
}

#include "ToolboxHitLedger.cpp"

void ToolboxScenePlatformController::emitHitEmitter(loka::core::EmitterState *emitter)
{
  if (!emitter)
  {
    return;
  }
  beginBatchUpdate();
  emitter->emit();
  endBatchUpdate();
}

bool ToolboxScenePlatformController::handleKeyDown(char key)
{
  EditTextControlBinding *focusedEdit = editControls_.focused();
  if (focusedEdit && focusedEdit->te)
  {
    beginBatchUpdate();
    TEKey(key, focusedEdit->te);
    updateStateFromEdit(*focusedEdit);
    endBatchUpdate();
    return true;
  }
  beginBatchUpdate();
  if (!handleTextKey(key))
  {
    endBatchUpdate();
    return false;
  }
  endBatchUpdate();
  return true;
}

void ToolboxScenePlatformController::applyPopupSelectionChange(const Rect &rect,
                                                               loka::app::scene::BoundaryNode *boundary,
                                                               loka::core::State<int> *selectedIndex,
                                                               loka::core::EmitterState *onChange,
                                                               int newIndex)
{
  (void)boundary;
  if (!selectedIndex)
  {
    return;
  }
  loka::core::MutableState<int> *mutableIndex =
      static_cast<loka::core::MutableState<int> *>(selectedIndex->asMutableState());
  if (!mutableIndex)
  {
    return;
  }
  beginBatchUpdate();
  addPendingDirty(rect);
  mutableIndex->set(newIndex, true);
  if (onChange)
  {
    onChange->emit();
  }
  endBatchUpdate();
}

bool ToolboxScenePlatformController::handleTextKey(char key)
{
  if (!focusedText_)
  {
    return false;
  }
  loka::core::MutableState<loka::core::String> *mutableText =
      static_cast<loka::core::MutableState<loka::core::String> *>(focusedText_->asMutableState());
  if (!mutableText)
  {
    return false;
  }
  std::string utf8;
  loka::platform::CollectUtf8(focusedText_->get(), utf8);
  if (key == 8 || key == 0x7F)
  {
    if (!utf8.empty())
    {
      utf8.erase(utf8.size() - 1);
    }
  }
  else if (key == 13)
  {
    return true;
  }
  else if (key >= 32)
  {
    utf8.push_back(key);
  }
  else
  {
    return false;
  }
  loka::core::StateTrackerGuard _(window_ ? window_->getTracker() : 0);
  mutableText->set(loka::core::String(utf8));
  return true;
}

void ToolboxScenePlatformController::bindTextState(loka::core::State<loka::core::String> *text)
{
  if (!text)
  {
    return;
  }
  for (size_t i = 0; i < boundTextStates_.size(); ++i)
  {
    if (boundTextStates_[i] == text)
    {
      return;
    }
  }
  boundTextStates_.push_back(text);
  TextBinding *binding = new TextBinding();
  binding->state = text;
  binding->controller = this;
  textBindings_.push_back(binding);
  text->bind(&ToolboxScenePlatformController::TextStateChangedThunk, binding, false, false, 0);
}

void ToolboxScenePlatformController::bindEnabledState(loka::core::State<bool> *enabled)
{
  this->enabledStateBindingPath_.bind(enabled);
}

void ToolboxScenePlatformController::unbindTextState(loka::core::State<loka::core::String> *text)
{
  for (size_t i = 0; i < boundTextStates_.size(); ++i)
  {
    if (boundTextStates_[i] != text)
    {
      continue;
    }
    TextBinding *binding = i < textBindings_.size() ? textBindings_[i] : 0;
    if (binding)
    {
      if (binding->state)
      {
        binding->state->unbind(&ToolboxScenePlatformController::TextStateChangedThunk, binding);
      }
      binding->state = 0;
      binding->controller = 0;
      delete binding;
    }
    boundTextStates_.erase(boundTextStates_.begin() + i);
    if (i < textBindings_.size())
    {
      textBindings_.erase(textBindings_.begin() + i);
    }
    return;
  }
}

void ToolboxScenePlatformController::unbindEnabledState(loka::core::State<bool> *enabled)
{
  this->enabledStateBindingPath_.unbind(enabled);
}

bool ToolboxScenePlatformController::hasLiveBinding(loka::core::State<loka::core::String> *text) const
{
  for (size_t i = 0; i < editControls_.size(); ++i)
  {
    if (editControls_[i].text == text)
    {
      return true;
    }
  }
  for (size_t i = 0; i < hitLedger_.cellHits_.size(); ++i)
  {
    if (hitLedger_.cellHits_[i].text == text)
    {
      return true;
    }
  }
  for (size_t i = 0; i < hitLedger_.editHits_.size(); ++i)
  {
    if (hitLedger_.editHits_[i].text == text)
    {
      return true;
    }
  }
  for (size_t i = 0; i < hitLedger_.textHits_.size(); ++i)
  {
    if (hitLedger_.textHits_[i].text == text)
    {
      return true;
    }
  }
  return false;
}

bool ToolboxScenePlatformController::hasLiveBinding(loka::core::State<bool> *enabled) const
{
  for (size_t i = 0; i < hitLedger_.buttonHits_.size(); ++i)
  {
    if (hitLedger_.buttonHits_[i].enabled == enabled)
    {
      return true;
    }
  }
  for (size_t i = 0; i < hitLedger_.popupHits_.size(); ++i)
  {
    if (hitLedger_.popupHits_[i].enabled == enabled)
    {
      return true;
    }
  }
  for (size_t i = 0; i < scrollBarLedger_.scrollBarControls_.size(); ++i)
  {
    if (scrollBarLedger_.scrollBarControls_[i].enabled == enabled)
    {
      return true;
    }
  }
  return false;
}

void ToolboxScenePlatformController::handleTextChanged(loka::core::State<loka::core::String> *text)
{
  if (!window_)
  {
    return;
  }
  // Native controls are state mirrors, not paint alternatives. Synchronize
  // every matching TE record before the first-match paint registries below can
  // return; a State may legitimately feed both an EditText and a Text/Cell.
  const size_t editControlMatchCount = editControls_.forEachTextBinding(
      text,
      this,
      &ToolboxScenePlatformController::refreshEditTextBindingForStateChange);
  for (size_t i = 0; i < hitLedger_.cellHits_.size(); ++i)
  {
    CellHit &hit = hitLedger_.cellHits_[i];
    if (hit.text == text)
    {
      ++debugStats_.textChangedCellCount;
      if (inBatchUpdate_)
      {
        addPendingDirty(hit.rect);
      }
      else
      {
        ++debugStats_.textChangedImmediateInvalidateCount;
        window_->requestInvalidateRect(hit.rect);
      }
      return;
    }
  }
  for (size_t i = 0; i < hitLedger_.textHits_.size(); ++i)
  {
    TextHit &hit = hitLedger_.textHits_[i];
    if (hit.text == text)
    {
      ++debugStats_.textChangedTextCount;
      if (hit.needsRelayoutOnChange)
      {
        ++debugStats_.relayoutTextCount;
        std::string utf8;
        if (loka::platform::CollectUtf8(text->get(), utf8))
        {
          if (utf8.size() > 48)
          {
            utf8.erase(48);
          }
          debugStats_.relayoutTextPreview = utf8;
        }
        else
        {
          debugStats_.relayoutTextPreview.clear();
        }
        if (inBatchUpdate_)
        {
          pendingFullInvalidate_ = true;
        }
        else
        {
          ++debugStats_.textChangedImmediateInvalidateCount;
          window_->requestInvalidateWithReason("text_relayout");
        }
        return;
      }
      short measuredWidth = ToolboxMeasureTextWidth(text->get());
      const short maxWidth = static_cast<short>(hit.rect.right - hit.rect.left);
      if (maxWidth > 0 && measuredWidth > maxWidth)
      {
        measuredWidth = maxWidth;
      }
      Rect dirtyRect = hit.rect;
      short redrawWidth = hit.lastMeasuredWidth;
      if (measuredWidth > redrawWidth)
      {
        redrawWidth = measuredWidth;
      }
      if (maxWidth > 0 && redrawWidth > maxWidth)
      {
        redrawWidth = maxWidth;
      }
      dirtyRect.right = static_cast<short>(dirtyRect.left + redrawWidth);
      if (inBatchUpdate_)
      {
        addPendingDirty(dirtyRect);
      }
      else
      {
        ++debugStats_.textChangedImmediateInvalidateCount;
        window_->requestInvalidateRect(dirtyRect);
      }
      return;
    }
  }
  for (size_t i = 0; i < hitLedger_.editHits_.size(); ++i)
  {
    EditHit &hit = hitLedger_.editHits_[i];
    if (hit.text == text)
    {
      ++debugStats_.textChangedEditHitCount;
      // Use text's own rect
      if (inBatchUpdate_)
      {
        addPendingDirty(hit.rect);
      }
      else
      {
        ++debugStats_.textChangedImmediateInvalidateCount;
        window_->requestInvalidateRect(hit.rect);
      }
      return;
    }
  }
  if (editControlMatchCount != 0)
  {
    return;
  }
  // State not found in current hitLedger_.textHits_/hitLedger_.editHits_/editControls_.
  // Add to pending list; will be resolved after next render populates hitLedger_.textHits_.
  if (inBatchUpdate_)
  {
    ++debugStats_.textChangedPendingCount;
    addPendingText(text);
  }
  else
  {
    // State not found, but scene invalidation will handle it
    // through normal recompose cycle. No need for full redraw.
  }
}

void ToolboxScenePlatformController::refreshEditTextBindingForStateChange(EditTextControlBinding &binding)
{
  ++debugStats_.textChangedEditControlCount;
  syncEditTextFromState(binding);
  if (inBatchUpdate_)
  {
    addPendingDirty(binding.rect);
    return;
  }
  ++debugStats_.textChangedImmediateInvalidateCount;
  window_->requestInvalidateRect(binding.rect);
}

bool ToolboxScenePlatformController::applyEnabledChangeForKind(
    ToolboxEnabledControlKind kind,
    loka::core::State<bool> *enabled)
{
  switch (kind)
  {
  case TOOLBOX_ENABLED_POPUP_HIT:
    for (size_t i = 0; i < hitLedger_.popupHits_.size(); ++i)
    {
      PopupHit &hit = hitLedger_.popupHits_[i];
      if (hit.enabled == enabled)
      {
        if (inBatchUpdate_)
        {
          addPendingDirty(hit.rect);
        }
        else
        {
          window_->requestInvalidateRect(hit.rect);
        }
        return true;
      }
    }
    return false;
  case TOOLBOX_ENABLED_BUTTON_CONTROL:
    for (size_t i = 0; i < buttonControls_.size(); ++i)
    {
      ButtonControlBinding &binding = buttonControls_[i];
      if (binding.enabled == enabled)
      {
        if (binding.control)
        {
          if (enabled->get())
          {
            HiliteControl(binding.control, 0);
          }
          else
          {
            HiliteControl(binding.control, 255);
          }
        }
        return true;
      }
    }
    return false;
  case TOOLBOX_ENABLED_SCROLL_BAR_CONTROL:
    for (size_t i = 0; i < scrollBarLedger_.scrollBarControls_.size(); ++i)
    {
      ScrollBarControlBinding &binding = scrollBarLedger_.scrollBarControls_[i];
      if (binding.enabled != enabled)
      {
        continue;
      }
      // A disabled bar and an unscrollable one share the inactive
      // presentation, so re-derive from both rather than from enabled alone.
      binding.active = enabled->get() && loka::app::ScrollBarIsScrollable(binding.minimum, binding.maximum);
      if (binding.control)
      {
        HiliteControl(binding.control, binding.active ? 0 : 255);
      }
      return true;
    }
    return false;
  case TOOLBOX_ENABLED_BUTTON_HIT:
    for (size_t i = 0; i < hitLedger_.buttonHits_.size(); ++i)
    {
      ButtonHit &hit = hitLedger_.buttonHits_[i];
      if (hit.enabled == enabled)
      {
        if (inBatchUpdate_)
        {
          addPendingDirty(hit.rect);
        }
        else
        {
          window_->requestInvalidateRect(hit.rect);
        }
        return true;
      }
    }
    return false;
  case TOOLBOX_ENABLED_CONTROL_KIND_COUNT:
    return false;
  }
  return false;
}

void ToolboxScenePlatformController::beginBatchUpdate()
{
  inBatchUpdate_ = true;
  pendingDirtyRects_.clear();
  pendingTextStates_.clear();
  pendingFullInvalidate_ = false;
  pendingInvalidateFlags_ = loka::app::scene::NODE_DIRTY_NONE;
  pendingRootNode_ = 0;
  debugStats_.batchAccumOnChangeCount = 0;
  debugStats_.batchAccumNullRootCount = 0;
  debugStats_.batchAccumFullRebuild = false;
  debugStats_.batchAccumFlags = loka::app::scene::NODE_DIRTY_NONE;
}

void ToolboxScenePlatformController::endBatchUpdate()
{
  inBatchUpdate_ = false;
  if (window_)
  {
    if (pendingRootNode_)
    {
      rootNode_ = pendingRootNode_;
    }
    const bool handledLocalDirty = !pendingDirtyRects_.empty();
    const bool handledLocalText = !pendingTextStates_.empty();
    const bool hasChildDirty = (pendingInvalidateFlags_ & loka::app::scene::NODE_DIRTY_CHILD) != 0;
    const bool skipFollowupInvalidate =
        !pendingFullInvalidate_ && !hasChildDirty && (handledLocalDirty || handledLocalText);
    // Record pending dirty rects; the app presents them after dispatch.
    for (size_t i = 0; i < pendingDirtyRects_.size(); ++i)
    {
      window_->requestInvalidateRect(pendingDirtyRects_[i]);
    }
    for (size_t i = 0; i < pendingTextStates_.size(); ++i)
    {
      requestInvalidateForText(pendingTextStates_[i]);
    }
    if (!skipFollowupInvalidate)
    {
      requestInvalidateForChange(
          pendingRootNode_ ? pendingRootNode_ : rootNode_, pendingInvalidateFlags_, pendingFullInvalidate_);
    }
  }
  pendingDirtyRects_.clear();
  pendingTextStates_.clear();
  pendingFullInvalidate_ = false;
  pendingInvalidateFlags_ = loka::app::scene::NODE_DIRTY_NONE;
  pendingRootNode_ = 0;
}

void ToolboxScenePlatformController::addPendingDirty(const Rect &rect)
{
  for (size_t i = 0; i < pendingDirtyRects_.size(); ++i)
  {
    Rect &pending = pendingDirtyRects_[i];
    if (rect.right < pending.left || rect.left > pending.right || rect.bottom < pending.top
        || rect.top > pending.bottom)
    {
      continue;
    }
    if (rect.left < pending.left)
    {
      pending.left = rect.left;
    }
    if (rect.top < pending.top)
    {
      pending.top = rect.top;
    }
    if (rect.right > pending.right)
    {
      pending.right = rect.right;
    }
    if (rect.bottom > pending.bottom)
    {
      pending.bottom = rect.bottom;
    }
    return;
  }
  pendingDirtyRects_.push_back(rect);
}

void ToolboxScenePlatformController::addPendingText(loka::core::State<loka::core::String> *text)
{
  if (!text)
  {
    return;
  }
  for (size_t i = 0; i < pendingTextStates_.size(); ++i)
  {
    if (pendingTextStates_[i] == text)
    {
      return;
    }
  }
  pendingTextStates_.push_back(text);
}

bool ToolboxScenePlatformController::collectLocalBoundaryDirtyRects(loka::app::scene::Node *node, const Rect &fallback)
{
  if (!node || !window_)
  {
    return false;
  }
  bool added = false;
  loka::app::scene::BoundaryNode *boundary = node->asBoundary();
  if (boundary && boundary->parentBoundary() && boundary->hasLayoutBounds() && boundary->canApplyLocalCompositionDiff())
  {
    ++debugStats_.rectInvalidateRequests;
    ++debugStats_.totalRectInvalidateRequests;
    window_->requestInvalidateRect(BoundaryToRect(boundary, fallback));
    added = true;
  }
  loka::app::scene::INestable *nestable = node->asNestable();
  if (!nestable)
  {
    return added;
  }
  loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
  for (loka::app::scene::Node *child = it.next(); child; child = it.next())
  {
    if (collectLocalBoundaryDirtyRects(child, fallback))
    {
      added = true;
    }
  }
  return added;
}

void ToolboxScenePlatformController::requestInvalidateForChange(loka::app::scene::Node *rootNodeForChange,
                                                                loka::app::scene::NodeDirtyFlags flags,
                                                                bool fullRebuild)
{
  if (debugStats_.requestInvalidateCallCount == 0)
  {
    debugStats_.requestInvalidateFirstRootPresent = (rootNodeForChange != 0);
    debugStats_.requestInvalidateFirstFullRebuild = fullRebuild;
    debugStats_.requestInvalidateFirstFlags = flags;
  }
  ++debugStats_.requestInvalidateCallCount;
  debugStats_.requestInvalidateRootPresent = (rootNodeForChange != 0);
  debugStats_.requestInvalidateFullRebuild = fullRebuild;
  debugStats_.requestInvalidateFlags = flags;
  if (!window_ || !window_->window())
  {
    return;
  }
  if ((flags == loka::app::scene::NODE_DIRTY_NONE) && !fullRebuild)
  {
    return;
  }
  if (flags & loka::app::scene::NODE_DIRTY_CHILD)
  {
    // Classic redraw currently prefers broad invalidation for child changes.
    // Narrow child-region invalidation remains below for future re-enable work.
    ++debugStats_.fullInvalidateRequests;
    ++debugStats_.totalFullInvalidateRequests;
    window_->requestInvalidateWithReason("child_dirty");
    return;
  }
  if (fullRebuild || !rootNodeForChange)
  {
    ++debugStats_.fullInvalidateRequests;
    ++debugStats_.totalFullInvalidateRequests;
    window_->requestInvalidateWithReason("full_rebuild_or_no_root");
    return;
  }

  Rect fallback = window_->window()->portRect;
  bool queued = false;
  if (flags & loka::app::scene::NODE_DIRTY_CHILD)
  {
    queued = collectLocalBoundaryDirtyRects(rootNodeForChange, fallback);
  }
  debugStats_.fallbackQueuedByChild = queued;
  if (!queued)
  {
    loka::app::scene::BoundaryNode *boundary = rootNodeForChange->asBoundary();
    debugStats_.fallbackRootIsBoundary = (boundary != 0);
    debugStats_.fallbackRootHasLayoutBounds = boundary && boundary->hasLayoutBounds();
    if (boundary && boundary->hasLayoutBounds())
    {
      ++debugStats_.rectInvalidateRequests;
      ++debugStats_.totalRectInvalidateRequests;
      window_->requestInvalidateRect(BoundaryToRect(boundary, fallback));
    }
    else
    {
      debugStats_.fallbackUsedFullInvalidate = true;
      ++debugStats_.rectInvalidateRequests;
      ++debugStats_.totalRectInvalidateRequests;
      window_->requestInvalidateRect(fallback);
    }
  }
}

std::string ToolboxScenePlatformController::debugStatsSummary() const
{
  return debugStats_.summary();
}

void ToolboxScenePlatformController::resetDebugStats()
{
  debugStats_.reset();
  // The pool counters are measurements too; leaving them cumulative would
  // let the next sync copy pre-reset activity into the fresh stats.
  pushButtonBucket_.resetCounters();
  textEditBucket_.resetCounters();
  poolIntakeAuditFailCount_ = 0;
  syncNativePoolStats();
}

bool ToolboxScenePlatformController::dumpDebugStatsToTimestampedFile() const
{
#if LOKA_RETRO68_DIAGNOSTICS
  return debugStats_.dumpToTimestampedFile();
#else
  // Compact profile (#135): the dump chain is compiled out; report failure.
  return false;
#endif
}

void ToolboxScenePlatformController::redrawTextHit(TextHit &hit)
{
  if (!window_ || !window_->window() || !hit.text)
  {
    return;
  }
  short measuredWidth = ToolboxMeasureTextWidth(hit.text->get());
  const short maxWidth = static_cast<short>(hit.rect.right - hit.rect.left);
  if (maxWidth > 0 && measuredWidth > maxWidth)
  {
    measuredWidth = maxWidth;
  }
  Rect dirtyRect = hit.rect;
  short redrawWidth = hit.lastMeasuredWidth;
  if (measuredWidth > redrawWidth)
  {
    redrawWidth = measuredWidth;
  }
  if (maxWidth > 0 && redrawWidth > maxWidth)
  {
    redrawWidth = maxWidth;
  }
  dirtyRect.right = static_cast<short>(dirtyRect.left + redrawWidth);
  GrafPtr oldPort;
  GetPort(&oldPort);
  SetPort(window_->window());
  EraseRect(&dirtyRect);
  RgnHandle oldClip = NewRgn();
  if (oldClip != 0)
  {
    GetClip(oldClip);
    ClipRect(&dirtyRect);
    DrawStringAt(hit.x, hit.y, hit.text->get());
    SetClip(oldClip);
    DisposeRgn(oldClip);
  }
  else
  {
    DrawStringAt(hit.x, hit.y, hit.text->get());
  }
  hit.lastMeasuredWidth = measuredWidth;
  SetPort(oldPort);
}

void ToolboxScenePlatformController::redrawPopupHit(const PopupHit &hit)
{
  if (!window_ || !window_->window())
  {
    return;
  }
  GrafPtr oldPort;
  GetPort(&oldPort);
  SetPort(window_->window());
  EraseRect(&hit.rect);
  loka::core::String label = loka::core::String::Literal("Select");
  int selectedIndex = 0;
  if (hit.selectedIndex)
  {
    selectedIndex = hit.selectedIndex->get();
  }
  if (hit.items && hit.items->size() > 0)
  {
    if (selectedIndex < 0)
    {
      selectedIndex = 0;
    }
    if (static_cast<std::size_t>(selectedIndex) >= hit.items->size())
    {
      selectedIndex = static_cast<int>(hit.items->size() - 1);
    }
    label = (*hit.items)[selectedIndex];
  }
  FrameRect(&hit.rect);
  PenState penState;
  GetPenState(&penState);
  PenPat(&qd.gray);
  MoveTo(hit.rect.left + 2, hit.rect.bottom);
  LineTo(hit.rect.right, hit.rect.bottom);
  LineTo(hit.rect.right, hit.rect.top + 2);
  SetPenState(&penState);
  short textY = static_cast<short>(hit.rect.top + hit.lineHeight - 2);
  DrawStringAt(static_cast<short>(hit.rect.left + 4), textY, label);
  short arrowRight = static_cast<short>(hit.rect.right - 4);
  short arrowTop = static_cast<short>(hit.rect.top + 4);
  short arrowBottom = static_cast<short>(hit.rect.bottom - 4);
  short arrowMidY = static_cast<short>((arrowTop + arrowBottom) / 2);
  MoveTo(static_cast<short>(arrowRight - 6), arrowMidY - 3);
  LineTo(arrowRight, arrowMidY - 3);
  LineTo(static_cast<short>(arrowRight - 3), arrowMidY + 3);
  LineTo(static_cast<short>(arrowRight - 6), arrowMidY - 3);
  SetPort(oldPort);
}

void ToolboxScenePlatformController::requestInvalidateForText(loka::core::State<loka::core::String> *text)
{
  if (!window_ || !text)
  {
    return;
  }
  for (size_t i = 0; i < hitLedger_.textHits_.size(); ++i)
  {
    if (hitLedger_.textHits_[i].text == text)
    {
      window_->requestInvalidateRect(hitLedger_.textHits_[i].rect);
      return;
    }
  }
}

void ToolboxScenePlatformController::clearTextBindings()
{
  for (size_t i = 0; i < textBindings_.size(); ++i)
  {
    TextBinding *binding = textBindings_[i];
    if (binding)
    {
      if (binding->state)
      {
        binding->state->unbind(&ToolboxScenePlatformController::TextStateChangedThunk, binding);
      }
      binding->state = 0;
      binding->controller = 0;
      delete binding;
    }
  }
  textBindings_.clear();
  boundTextStates_.clear();
  hitLedger_.textHits_.clear();
}

void ToolboxScenePlatformController::clearEnabledBindings()
{
  this->enabledStateBindingPath_.clear();
}

void ToolboxScenePlatformController::clearControls()
{
  for (size_t i = 0; i < buttonControls_.size(); ++i)
  {
    if (buttonControls_[i].control)
    {
      HideControl(buttonControls_[i].control);
      queueRetiredControl(buttonControls_[i].control, buttonControls_[i].lifetimeHint);
      buttonControls_[i].control = 0;
    }
  }
  buttonControls_.clear();
  for (size_t i = 0; i < scrollBarLedger_.scrollBarControls_.size(); ++i)
  {
    if (scrollBarLedger_.scrollBarControls_[i].control)
    {
      HideControl(scrollBarLedger_.scrollBarControls_[i].control);
      queueRetiredScrollBarControl(scrollBarLedger_.scrollBarControls_[i].control, scrollBarLedger_.scrollBarControls_[i].lifetimeHint);
      scrollBarLedger_.scrollBarControls_[i].control = 0;
    }
  }
  scrollBarLedger_.scrollBarControls_.clear();
  scrollBarLedger_.viewportScrollBars_.clear();
  for (size_t i = 0; i < editControls_.size(); ++i)
  {
    if (editControls_[i].te)
    {
      TEDeactivate(editControls_[i].te);
      queueRetiredTextEdit(editControls_[i].te, editControls_[i].lifetimeHint);
      editControls_[i].te = 0;
    }
  }
  editControls_.clear();
}

template <typename HandleT>
void ToolboxScenePlatformController::queueRetiredNativeHandle(std::vector<RetiredNativeEntry<HandleT> > &retired,
                                                              HandleT handle,
                                                              loka::app::scene::NativeLifetimeHint lifetimeHint)
{
  if (!handle)
  {
    return;
  }
  for (size_t i = 0; i < retired.size(); ++i)
  {
    if (retired[i].handle == handle)
    {
      return;
    }
  }
  RetiredNativeEntry<HandleT> entry;
  entry.handle = handle;
  entry.lifetimeHint = lifetimeHint;
  retired.push_back(entry);
}

void ToolboxScenePlatformController::queueRetiredControl(ControlRef control,
                                                         loka::app::scene::NativeLifetimeHint lifetimeHint)
{
  queueRetiredNativeHandle(retiredControls_, control, lifetimeHint);
}

void ToolboxScenePlatformController::queueRetiredScrollBarControl(ControlRef control,
                                                                   loka::app::scene::NativeLifetimeHint lifetimeHint)
{
  queueRetiredNativeHandle(retiredScrollBarControls_, control, lifetimeHint);
}

void ToolboxScenePlatformController::queueRetiredTextEdit(TEHandle te,
                                                          loka::app::scene::NativeLifetimeHint lifetimeHint)
{
  queueRetiredNativeHandle(retiredTextEdits_, te, lifetimeHint);
}

bool ToolboxScenePlatformController::hasLiveBinding(ControlRef control) const
{
  for (size_t i = 0; i < buttonControls_.size(); ++i)
  {
    if (buttonControls_[i].control == control)
    {
      return true;
    }
  }
  for (size_t i = 0; i < scrollBarLedger_.scrollBarControls_.size(); ++i)
  {
    if (scrollBarLedger_.scrollBarControls_[i].control == control)
    {
      return true;
    }
  }
  return false;
}

bool ToolboxScenePlatformController::hasLiveBinding(TEHandle te) const
{
  for (size_t i = 0; i < editControls_.size(); ++i)
  {
    if (editControls_[i].te == te)
    {
      return true;
    }
  }
  return false;
}

void ToolboxScenePlatformController::disposeNativeHandle(ControlRef control)
{
  if (control)
  {
    DisposeControl(control);
  }
}

void ToolboxScenePlatformController::disposeNativeHandle(TEHandle te)
{
  if (te)
  {
    TEDispose(te);
  }
}

template <typename HandleT>
void ToolboxScenePlatformController::flushRetiredEntriesInto(
    std::vector<RetiredNativeEntry<HandleT> > &retired,
    loka::app::scene::ExactMatchHandleBucket<HandleT> &bucket)
{
  for (size_t i = 0; i < retired.size(); ++i)
  {
    HandleT handle = retired[i].handle;
    if (!handle)
    {
      continue;
    }
    if (retired[i].lifetimeHint == loka::app::scene::NATIVE_HINT_EAGER_RELEASE)
    {
      disposeNativeHandle(handle);
      continue;
    }
    // Bag entries must hold zero pointers into Loka; a live binding still
    // referencing the handle means the retire ritual did not complete.
    // Leaking the handle (counted) is the safe arm — disposing it would
    // hand the live binding a dead handle, and pooling it would pay the
    // same handle out twice.
    if (hasLiveBinding(handle))
    {
      ++poolIntakeAuditFailCount_;
      continue;
    }
    if (!bucket.offer(handle))
    {
      disposeNativeHandle(handle);
    }
  }
  retired.clear();
}

void ToolboxScenePlatformController::flushRetiredNativeHandles()
{
  flushRetiredEntriesInto(retiredControls_, pushButtonBucket_);
  flushRetiredEntriesInto(retiredScrollBarControls_, scrollBarLedger_.scrollBarBucket_);
  flushRetiredEntriesInto(retiredTextEdits_, textEditBucket_);
  syncNativePoolStats();
}

namespace
{
  void DisposePooledControl(ControlRef control)
  {
    if (control)
    {
      DisposeControl(control);
    }
  }

  void DisposePooledTextEdit(TEHandle te)
  {
    if (te)
    {
      TEDispose(te);
    }
  }
} // namespace

void ToolboxScenePlatformController::drainNativeHandleBuckets()
{
  pushButtonBucket_.drainWith(DisposePooledControl);
  scrollBarLedger_.scrollBarBucket_.drainWith(DisposePooledControl);
  textEditBucket_.drainWith(DisposePooledTextEdit);
  syncNativePoolStats();
}

void ToolboxScenePlatformController::syncNativePoolStats()
{
  debugStats_.refreshNativePoolCounters(pushButtonBucket_.hitCount(),
                                        pushButtonBucket_.missCount(),
                                        pushButtonBucket_.evictCount(),
                                        static_cast<int>(pushButtonBucket_.depth()),
                                        textEditBucket_.hitCount(),
                                        textEditBucket_.missCount(),
                                        textEditBucket_.evictCount(),
                                        static_cast<int>(textEditBucket_.depth()),
                                        poolIntakeAuditFailCount_);
}

bool ToolboxScenePlatformController::ensureButtonControl(short resourceId,
                                                         const Rect &rect,
                                                         const loka::core::String &label,
                                                         loka::core::EmitterState *emitter,
                                                         loka::core::State<bool> *enabled,
                                                         loka::app::scene::NativeLifetimeHint lifetimeHint)
{
  if (!window_ || !window_->window() || resourceId <= 0)
  {
    return false;
  }
  Rect controlRect;
  if (!this->intersectWithProjectionClip(rect, controlRect))
  {
    return true;
  }
  ButtonControlBinding *binding = 0;
  for (size_t i = 0; i < buttonControls_.size(); ++i)
  {
    if (buttonControls_[i].resourceId == resourceId)
    {
      binding = &buttonControls_[i];
      break;
    }
  }
  bool created = false;
  if (!binding)
  {
    ControlRef control = 0;
    if (pushButtonBucket_.tryAcquire(control))
    {
      // Pooled handles keep their last title; restore the fresh-created
      // blank so the label diff below runs against the same baseline.
      Str255 title;
      title[0] = 0;
      SetControlTitle(control, title);
    }
    else
    {
      Rect rectCopy = controlRect;
      Str255 title;
      title[0] = 0;
      control = NewControl(window_->window(), &rectCopy, title, false, 0, 0, 1, pushButProc, 0);
      if (!control)
      {
        return false;
      }
      HideControl(control);
    }
    ButtonControlBinding entry;
    entry.resourceId = resourceId;
    entry.control = control;
    entry.emitter = emitter;
    entry.enabled = enabled;
    entry.usedThisFrame = true;
    entry.needsDraw = true;
    entry.rect = controlRect;
    entry.label = "";
    entry.lifetimeHint = lifetimeHint;
    buttonControls_.push_back(entry);
    binding = &buttonControls_.back();
    created = true;
  }
  binding->emitter = emitter;
  binding->enabled = enabled;
  binding->lifetimeHint = lifetimeHint;
  bindEnabledState(enabled);
  binding->usedThisFrame = true;
  if (created || binding->rect.left != controlRect.left || binding->rect.top != controlRect.top || binding->rect.right != controlRect.right
      || binding->rect.bottom != controlRect.bottom)
  {
    MoveControl(binding->control, controlRect.left, controlRect.top);
    SizeControl(binding->control, controlRect.right - controlRect.left, controlRect.bottom - controlRect.top);
    binding->rect = controlRect;
    binding->needsDraw = true;
  }
  std::string labelUtf8;
  if (!loka::platform::CollectUtf8(label, labelUtf8))
  {
    labelUtf8.clear();
  }
  if (binding->label != labelUtf8)
  {
    Str255 title;
    CopyToPascalString(label, title);
    SetControlTitle(binding->control, title);
    binding->label = labelUtf8;
    binding->needsDraw = true;
  }
  if (binding->enabled && !binding->enabled->get())
  {
    HiliteControl(binding->control, 255);
  }
  else
  {
    HiliteControl(binding->control, 0);
  }
  ShowControl(binding->control);
  return true;
}

void ToolboxScenePlatformController::destroyButtonControl(short resourceId,
                                                           loka::app::scene::NativeLifetimeHint lifetimeHint)
{
  for (size_t i = 0; i < buttonControls_.size(); ++i)
  {
    ButtonControlBinding &binding = buttonControls_[i];
    if (binding.resourceId != resourceId)
    {
      continue;
    }
    ControlRef control = binding.control;
    binding.control = 0;
    binding.emitter = 0;
    binding.enabled = 0;
    buttonControls_.erase(buttonControls_.begin() + i);
    controlIds_.release(resourceId);
    if (control)
    {
      // Context destruction can run inside an update pass; disposal waits for
      // the platform safe point like every other retired native handle.
      HideControl(control);
      queueRetiredControl(control, lifetimeHint);
    }
    return;
  }
}

#include "ToolboxScrollBarLedger.cpp"

void ToolboxScenePlatformController::drawFallbackControl(const Rect &rect)
{
  FrameRect(&rect);
  MoveTo(rect.left + 2, rect.top + 2);
  LineTo(rect.right - 2, rect.bottom - 2);
  MoveTo(rect.left + 2, rect.bottom - 2);
  LineTo(rect.right - 2, rect.top + 2);
}

TEHandle ToolboxScenePlatformController::ensureEditTextControl(ToolboxEditTextContext *ownerContext,
                                                               const Rect &rect,
                                                               loka::core::State<loka::core::String> *text,
                                                               loka::app::scene::NativeLifetimeHint lifetimeHint)
{
  if (!text || !ownerContext)
  {
    return 0;
  }
  Rect controlRect;
  if (!this->intersectWithProjectionClip(rect, controlRect))
  {
    return 0;
  }
  EditTextControlBinding *binding = 0;
  loka::core::State<loka::core::String> *previousText = 0;
  size_t bindingIndex = 0;
  if (editControls_.find(ownerContext, bindingIndex))
  {
    binding = &editControls_[bindingIndex];
    previousText = binding->text;
    binding->text = text;
  }
  if (!binding)
  {
    TEHandle te = 0;
    if (textEditBucket_.tryAcquire(te))
    {
      // Restore the fresh-created baseline: pooled records keep their last
      // text and rects, and the new binding starts from lastText == "".
      TESetText(static_cast<const void *>(""), 0, te);
      (**te).destRect = controlRect;
      (**te).viewRect = controlRect;
      TECalText(te);
    }
    else
    {
      te = TENew(&controlRect, &controlRect);
      if (!te)
      {
        return 0;
      }
    }
    EditTextControlBinding entry;
    entry.ownerContext = ownerContext;
    entry.text = text;
    entry.te = te;
    entry.rect = controlRect;
    entry.usedThisFrame = true;
    entry.lastText = "";
    entry.lifetimeHint = lifetimeHint;
    editControls_.add(entry);
    binding = &editControls_.back();
    syncEditTextFromState(*binding);
    TEAutoView(true, binding->te);
  }
  // A native EditText is a live String projection just like Text and the
  // fallback EditHit. Register it at the same seam so programmatic writes can
  // reach TESetText even when no render walk follows the write.
  bindTextState(text);
  if (previousText && previousText != text && !this->hasLiveBinding(previousText))
  {
    unbindTextState(previousText);
  }
  binding->usedThisFrame = true;
  binding->lifetimeHint = lifetimeHint;
  if (binding->rect.left != controlRect.left || binding->rect.top != controlRect.top || binding->rect.right != controlRect.right
      || binding->rect.bottom != controlRect.bottom)
  {
    binding->rect = controlRect;
    if (binding->te)
    {
      (**binding->te).destRect = controlRect;
      (**binding->te).viewRect = controlRect;
      TECalText(binding->te);
      TEAutoView(true, binding->te);
    }
  }
  if (!inBatchUpdate_)
  {
    syncEditTextFromState(*binding);
  }
  return binding ? binding->te : 0;
}

void ToolboxScenePlatformController::retireEditTextControlAt(
    std::size_t index,
    loka::app::scene::NativeLifetimeHint lifetimeHint)
{
  EditTextControlBinding &binding = editControls_[index];
  loka::core::State<loka::core::String> *retiredText = binding.text;
  if (binding.te)
  {
    TEDeactivate(binding.te);
    queueRetiredTextEdit(binding.te, lifetimeHint);
    binding.te = 0;
  }
  editControls_.erase(index);
  if (retiredText && !this->hasLiveBinding(retiredText))
  {
    unbindTextState(retiredText);
  }
}

void ToolboxScenePlatformController::retireEditTextControl(
    loka::app::scene::NodeContext *ownerContext,
    loka::app::scene::NativeLifetimeHint lifetimeHint)
{
  std::size_t index = 0;
  if (editControls_.find(ownerContext, index))
  {
    this->retireEditTextControlAt(index, lifetimeHint);
  }
}

void ToolboxScenePlatformController::syncEditTextFromState(EditTextControlBinding &binding)
{
  if (!binding.text || !binding.te)
  {
    return;
  }
  std::string utf8;
  loka::platform::CollectUtf8(binding.text->get(), utf8);
  if (binding.lastText == utf8)
  {
    return;
  }
  TESetText(utf8.c_str(), static_cast<long>(utf8.size()), binding.te);
  TESetSelect(utf8.size(), utf8.size(), binding.te);
  binding.lastText = utf8;
}

#ifdef TEST_BUILD
bool ToolboxScenePlatformController::queryEditTextValueForTesting(
    ToolboxEditTextContext *ownerContext,
    std::string &out) const
{
  out.clear();
  size_t index = 0;
  if (!ownerContext || !editControls_.find(ownerContext, index))
  {
    return false;
  }
  const EditTextControlBinding &binding = editControls_[index];
  if (!binding.te || !*binding.te)
  {
    return false;
  }
  // TERec::hText is CharsHandle (unsigned char **) under Apple's Universal
  // Interfaces and Handle (char **) under Multiversal, and both toolchains are
  // supported. Reach it as a plain Handle so the declaration this file sees
  // does not decide whether it compiles.
  Handle textHandle = reinterpret_cast<Handle>((**binding.te).hText);
  const long length = (**binding.te).teLength;
  if (length < 0 || (length > 0 && !textHandle))
  {
    return false;
  }
  if (length > 0)
  {
    const char previousHandleState = HGetState(textHandle);
    HLock(textHandle);
    const char *bytes = reinterpret_cast<const char *>(*textHandle);
    if (!bytes)
    {
      HSetState(textHandle, previousHandleState);
      return false;
    }
    out.assign(bytes, static_cast<std::string::size_type>(length));
    HSetState(textHandle, previousHandleState);
  }
  return true;
}
#endif

void ToolboxScenePlatformController::updateStateFromEdit(EditTextControlBinding &binding)
{
  if (!binding.text || !binding.te)
  {
    return;
  }
  loka::core::MutableState<loka::core::String> *mutableText =
      static_cast<loka::core::MutableState<loka::core::String> *>(binding.text->asMutableState());
  if (!mutableText)
  {
    return;
  }
  CharsHandle textHandle = TEGetText(binding.te);
  long length = 0;
  if (binding.te && *binding.te)
  {
    length = (**binding.te).teLength;
  }
  std::string utf8;
  if (textHandle && length > 0)
  {
    HLock(reinterpret_cast<Handle>(textHandle));
    const char *ptr = reinterpret_cast<const char *>(*textHandle);
    utf8.assign(ptr, static_cast<size_t>(length));
    HUnlock(reinterpret_cast<Handle>(textHandle));
  }
  loka::core::StateTrackerGuard _(window_ ? window_->getTracker() : 0);
  // State notification fans out to every binding. Mark the typing source
  // current first so its sync is a no-op and preserves the active selection.
  binding.lastText = utf8;
  mutableText->set(loka::core::String(utf8));
}

void ToolboxScenePlatformController::drawControlsInRect(const Rect &rect)
{
  for (size_t i = 0; i < buttonControls_.size(); ++i)
  {
    ButtonControlBinding &binding = buttonControls_[i];
    if (!binding.control || !binding.usedThisFrame)
    {
      continue;
    }
    if (rect.right < binding.rect.left || rect.left > binding.rect.right || rect.bottom < binding.rect.top
        || rect.top > binding.rect.bottom)
    {
      continue;
    }
    Draw1Control(binding.control);
    ++debugStats_.controlDrawCount;
    ++debugStats_.totalControlDrawCount;
    binding.needsDraw = false;
  }
  for (size_t i = 0; i < scrollBarLedger_.scrollBarControls_.size(); ++i)
  {
    ScrollBarControlBinding &binding = scrollBarLedger_.scrollBarControls_[i];
    if (!binding.control || !binding.usedThisFrame)
    {
      continue;
    }
    if (rect.right < binding.rect.left || rect.left > binding.rect.right || rect.bottom < binding.rect.top
        || rect.top > binding.rect.bottom)
    {
      continue;
    }
    Draw1Control(binding.control);
    ++debugStats_.controlDrawCount;
    ++debugStats_.totalControlDrawCount;
  }
}

void ToolboxScenePlatformController::idleTextEdits()
{
  for (size_t i = 0; i < editControls_.size(); ++i)
  {
    if (editControls_[i].te)
    {
      TEIdle(editControls_[i].te);
    }
  }
}

bool ToolboxScenePlatformController::isPointInEdit(const Point &point) const
{
  for (size_t i = 0; i < editControls_.size(); ++i)
  {
    const EditTextControlBinding &binding = editControls_[i];
    if (binding.te && PtInRect(point, &binding.rect))
    {
      return true;
    }
  }
  for (size_t i = 0; i < hitLedger_.editHits_.size(); ++i)
  {
    const EditHit &hit = hitLedger_.editHits_[i];
    if (hit.text && PtInRect(point, &hit.rect))
    {
      return true;
    }
  }
  return false;
}

bool ToolboxScenePlatformController::intersectWithProjectionClip(
    const Rect &rect,
    Rect &clipped) const
{
  if (this->projectionParentScopes_.activeDepth() == 0)
  {
    clipped = rect;
    return clipped.left < clipped.right && clipped.top < clipped.bottom;
  }
  const loka::core::Frame &clip =
      this->projectionParentScopes_.current().clipRect;
  Rect viewport;
  viewport.left = static_cast<short>(clip.x);
  viewport.top = static_cast<short>(clip.y);
  viewport.right = static_cast<short>(clip.x + clip.width);
  viewport.bottom = static_cast<short>(clip.y + clip.height);
  return SectRect(&rect, &viewport, &clipped) != 0 &&
         clipped.left < clipped.right && clipped.top < clipped.bottom;
}

void ToolboxScenePlatformController::beginClip(const Rect &rect)
{
  if (clipRgn_)
  {
    GetClip(clipRgn_);
    ClipRect(&rect);
    hasClip_ = true;
  }
}

void ToolboxScenePlatformController::endClip()
{
  if (clipRgn_ && hasClip_)
  {
    SetClip(clipRgn_);
    hasClip_ = false;
  }
}

void ToolboxScenePlatformController::TextStateChangedThunk(void *userData)
{
  TextBinding *binding = static_cast<TextBinding *>(userData);
  if (!binding || !binding->controller || !binding->state)
  {
    return;
  }
  binding->controller->handleTextChanged(binding->state);
}
