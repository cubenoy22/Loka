#include "Win32ScenePlatformController.hpp"
#include "Win32BuiltInSupport.hpp"
#include "app/scene/boundary/Boundary.hpp"
#include <windows.h>
#include <cstdio>
#include <map>
#include <vector>
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/nestable/Grid.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/ZStack.hpp"
#include "app/RectSurface.hpp"
#include "app/layout/LayoutHeuristics.hpp"
#include "app/layout/PlatformBuiltinLayoutHandlers.hpp"
#include "app/layout/BoxLayout.hpp"
#include "app/layout/ColumnLayout.hpp"
#include "app/layout/GridLayout.hpp"
#include "app/layout/RowLayout.hpp"
#include "app/layout/ZStackLayout.hpp"
#include "app/scene/Node.hpp"
#include "core/Profiler.hpp"
#include "context/Win32ButtonContext.hpp"
#include "context/Win32EditTextContext.hpp"
#include "context/Win32CellContext.hpp"
#include "context/Win32PopupMenuContext.hpp"
#include "context/Win32ImageViewContext.hpp"
#include "context/Win32OpenFileDialogContext.hpp"
#include "context/Win32RectSurfaceContext.hpp"

namespace
{
  typedef std::map<HWND, Win32ScenePlatformController *> Win32ControllerMap;
  Win32ControllerMap gControllersByRootHwnd;

  const int kButtonHeight = 32;
  const int kEditTextHeight = 24;
  const int kPopupMenuHeight = 26;
  const int kTextHeight = 20;
  const int kVerticalSpacing = 12;
  const int kHorizontalSpacing = 12;
  const int kImageFallbackHeightModern = 160;

} // namespace

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class Win32PlatformLayoutTraversal : public IPlatformLayoutTraversal
      {
      public:
        explicit Win32PlatformLayoutTraversal(Win32ScenePlatformController *controller)
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
          this->layoutResultY_ = state.y;
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
        Win32ScenePlatformController *controller_;
        short layoutResultY_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

Win32ScenePlatformController::Win32ScenePlatformController(HWND rootHwnd)
    : rootHwnd_(rootHwnd),
      contextMapper_(rootHwnd),
      rootNode_(0),
      clientWidth_(0),
      clientHeight_(0)
{
  RegisterWin32BuiltInSupport(*this);
  if (rootHwnd_)
  {
    gControllersByRootHwnd[rootHwnd_] = this;
  }
}

Win32ScenePlatformController::~Win32ScenePlatformController()
{
  if (rootHwnd_)
  {
    Win32ControllerMap::iterator it = gControllersByRootHwnd.find(rootHwnd_);
    if (it != gControllersByRootHwnd.end() && it->second == this)
    {
      gControllersByRootHwnd.erase(it);
    }
  }
  clearContexts();
}

void Win32ScenePlatformController::requestDirtyRect(HWND targetHwnd, const RECT *rect, BOOL eraseBackground)
{
  if (!targetHwnd)
  {
    return;
  }
  HWND root = GetAncestor(targetHwnd, GA_ROOT);
  if (!root)
  {
    root = targetHwnd;
  }
  Win32ControllerMap::iterator it = gControllersByRootHwnd.find(root);
  if (it == gControllersByRootHwnd.end() || !it->second)
  {
    InvalidateRect(targetHwnd, rect, eraseBackground);
    return;
  }
  it->second->queueDirtyRect(targetHwnd, rect, eraseBackground, false);
}

bool Win32ScenePlatformController::registerNodeHandler(loka::app::scene::IPlatformNodeHandler *handler)
{
  return this->nodeHandlerRegistry_.registerHandler(handler);
}

bool Win32ScenePlatformController::prepareProjectedLayout(loka::app::scene::Node *node,
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

int Win32ScenePlatformController::layoutNodeFromSceneState(loka::app::scene::Node *node,
                                                           const loka::app::scene::LayoutState &state)
{
  LayoutState localState;
  localState.x = state.x;
  localState.y = state.y;
  localState.width = state.width;
  localState.height = state.height;
  return this->layoutNode(node, localState);
}

void Win32ScenePlatformController::requestDirtySubtree(HWND targetHwnd, const RECT *rect, BOOL eraseBackground)
{
  if (!targetHwnd)
  {
    return;
  }
  HWND root = GetAncestor(targetHwnd, GA_ROOT);
  if (!root)
  {
    root = targetHwnd;
  }
  Win32ControllerMap::iterator it = gControllersByRootHwnd.find(root);
  if (it == gControllersByRootHwnd.end() || !it->second)
  {
    UINT flags = RDW_INVALIDATE | (eraseBackground ? RDW_ERASE : 0) | RDW_ALLCHILDREN;
    RedrawWindow(targetHwnd, rect, NULL, flags);
    return;
  }
  it->second->queueDirtyRect(targetHwnd, rect, eraseBackground, true);
}

void Win32ScenePlatformController::redrawDirtySubtreeNow(HWND targetHwnd, const RECT *rect, BOOL eraseBackground)
{
  if (!targetHwnd)
  {
    return;
  }
  UINT flags = RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW;
  if (eraseBackground)
  {
    flags |= RDW_ERASE;
  }
  RedrawWindow(targetHwnd, rect, NULL, flags);
}

void Win32ScenePlatformController::noteNativePaint(HWND targetHwnd, NativePaintKind kind, bool eraseBackground)
{
  if (!targetHwnd)
  {
    return;
  }
  HWND root = GetAncestor(targetHwnd, GA_ROOT);
  if (!root)
  {
    root = targetHwnd;
  }
  Win32ControllerMap::iterator it = gControllersByRootHwnd.find(root);
  if (it == gControllersByRootHwnd.end() || !it->second)
  {
    return;
  }
  RedrawStats &stats = it->second->redrawStats_;
  switch (kind)
  {
  case NATIVE_PAINT_ROOT:
    if (eraseBackground)
    {
      ++stats.rootEraseCount;
    }
    else
    {
      ++stats.rootPaintCount;
    }
    break;
  case NATIVE_PAINT_CELL:
    if (eraseBackground)
    {
      ++stats.cellEraseCount;
    }
    else
    {
      ++stats.cellPaintCount;
    }
    break;
  case NATIVE_PAINT_IMAGE:
    if (eraseBackground)
    {
      ++stats.imageEraseCount;
    }
    else
    {
      ++stats.imagePaintCount;
    }
    break;
  case NATIVE_PAINT_RECT_SURFACE:
    if (eraseBackground)
    {
      ++stats.rectSurfaceEraseCount;
    }
    else
    {
      ++stats.rectSurfacePaintCount;
    }
    break;
  default:
    break;
  }
}

void Win32ScenePlatformController::onChange(loka::app::scene::Node *rootNode,
                                            loka::app::scene::NodeDirtyFlags flags,
                                            bool fullRebuild)
{
  ++this->redrawStats_.onChangeCalls;
  this->redrawStats_.lastOnChangeFlags = flags;
  this->redrawStats_.lastOnChangeFullRebuild = fullRebuild;
  rootNode_ = rootNode;
  if (!rootHwnd_ || !rootNode_)
  {
    this->redrawStats_.lastOnChangeRequiredLayout = false;
    return;
  }

  const bool requiresLayout = (flags & loka::app::scene::NODE_DIRTY_INITIAL) != 0
                              || (flags & loka::app::scene::NODE_DIRTY_LAYOUT) != 0
                              || (flags & loka::app::scene::NODE_DIRTY_CHILD) != 0;
  this->redrawStats_.lastOnChangeRequiredLayout = requiresLayout;
  if (!requiresLayout)
  {
    return;
  }

  RECT rc;
  if (GetClientRect(rootHwnd_, &rc))
  {
    clientWidth_ = rc.right - rc.left;
    clientHeight_ = rc.bottom - rc.top;
  }
  performLayout(clientWidth_, clientHeight_, fullRebuild);
}

void Win32ScenePlatformController::onBoundaryApply(loka::app::scene::Node *rootNode,
                                                   loka::app::scene::BoundaryNode *boundary,
                                                   const loka::app::scene::BoundaryLocalApplyInfo &info,
                                                   const loka::app::scene::PlatformApplyPlan &plan)
{
  ++this->redrawStats_.onBoundaryApplyCalls;
  if (rootNode)
  {
    rootNode_ = rootNode;
  }
  if (!rootHwnd_ || !rootNode_ || !boundary || !plan.hasBoundaryApplyWork(boundary))
  {
    return;
  }
  if (info.hasStructureWork || info.hasLayoutWork || !info.hasPaintWork())
  {
    return;
  }

  const bool eraseBackground = !info.paintIsOpaque;
  const bool includeChildren = info.hasCompositedPaintWork();
  if (info.hasCompositedPaintWork())
  {
    ++this->redrawStats_.queuedCompositedInvalidates;
  }
  else if (info.hasOpaquePaintWork())
  {
    ++this->redrawStats_.queuedOpaquePaintInvalidates;
  }
  else if (info.hasPaintWork())
  {
    ++this->redrawStats_.queuedGenericPaintInvalidates;
  }
  if (!info.hasBoundsHint())
  {
    ++this->redrawStats_.queuedFullWindowInvalidates;
    ++this->redrawStats_.queuedMissingBoundsInvalidates;
    queueDirtyRect(rootHwnd_, 0, eraseBackground ? TRUE : FALSE, includeChildren);
    return;
  }

  ++this->redrawStats_.queuedRectInvalidates;
  if (info.hasPaintBoundsHint())
  {
    ++this->redrawStats_.queuedPaintBoundsInvalidates;
  }
  else if (info.hasLayoutBoundsHint())
  {
    ++this->redrawStats_.queuedLayoutBoundsInvalidates;
  }
  RECT rect;
  rect.left = info.bounds->x;
  rect.top = info.bounds->y;
  rect.right = info.bounds->x + info.bounds->width;
  rect.bottom = info.bounds->y + info.bounds->height;
  queueDirtyRect(rootHwnd_, &rect, eraseBackground ? TRUE : FALSE, includeChildren);
}

void Win32ScenePlatformController::beginApplyCycle()
{
  this->redrawStats_.reset();
}

void Win32ScenePlatformController::synchronize()
{
  dumpRedrawStatsIfNeeded();
  for (size_t i = 0; i < pendingInvalidations_.size(); ++i)
  {
    PendingInvalidate &entry = pendingInvalidations_[i];
    if (!IsWindow(entry.hwnd))
    {
      continue;
    }
    UINT flags = RDW_INVALIDATE | RDW_UPDATENOW;
    if (entry.eraseBackground)
    {
      flags |= RDW_ERASE;
    }
    if (entry.includeChildren)
    {
      flags |= RDW_ALLCHILDREN;
    }
    RedrawWindow(entry.hwnd, entry.fullWindow ? NULL : &entry.rect, NULL, flags);
  }
  pendingInvalidations_.clear();
  redrawStats_.reset();
}

bool Win32ScenePlatformController::hasPendingSync() const
{
  return !pendingInvalidations_.empty();
}

void Win32ScenePlatformController::destroy()
{
  pendingInvalidations_.clear();
  clearContexts();
  rootNode_ = 0;
  clientWidth_ = 0;
  clientHeight_ = 0;
}

void Win32ScenePlatformController::releaseNodeContexts(loka::app::scene::Node *node)
{
  clearNodeContexts(node);
}

void Win32ScenePlatformController::queueDirtyRect(HWND targetHwnd,
                                                  const RECT *rect,
                                                  BOOL eraseBackground,
                                                  bool includeChildren)
{
  if (!targetHwnd)
  {
    return;
  }
  for (size_t i = 0; i < pendingInvalidations_.size(); ++i)
  {
    PendingInvalidate &entry = pendingInvalidations_[i];
    if (entry.hwnd != targetHwnd)
    {
      continue;
    }
    entry.eraseBackground = (entry.eraseBackground || eraseBackground) ? TRUE : FALSE;
    entry.includeChildren = entry.includeChildren || includeChildren;
    if (!rect)
    {
      entry.fullWindow = true;
      return;
    }
    if (entry.fullWindow)
    {
      return;
    }
    if (rect->left < entry.rect.left)
    {
      entry.rect.left = rect->left;
    }
    if (rect->top < entry.rect.top)
    {
      entry.rect.top = rect->top;
    }
    if (rect->right > entry.rect.right)
    {
      entry.rect.right = rect->right;
    }
    if (rect->bottom > entry.rect.bottom)
    {
      entry.rect.bottom = rect->bottom;
    }
    return;
  }

  PendingInvalidate entry;
  entry.hwnd = targetHwnd;
  entry.eraseBackground = eraseBackground;
  entry.includeChildren = includeChildren;
  if (!rect)
  {
    entry.fullWindow = true;
  }
  else
  {
    entry.rect = *rect;
  }
  pendingInvalidations_.push_back(entry);
}

void Win32ScenePlatformController::dumpRedrawStatsIfNeeded()
{
#if defined(LOKA_DEBUG_RECOMPOSE) && !defined(LOKA_RETRO68)
  if (redrawStats_.onChangeCalls == 0 && redrawStats_.onBoundaryApplyCalls == 0
      && redrawStats_.queuedFullWindowInvalidates == 0 && redrawStats_.queuedRectInvalidates == 0
      && redrawStats_.rootEraseCount == 0 && redrawStats_.rootPaintCount == 0 && redrawStats_.cellEraseCount == 0
      && redrawStats_.cellPaintCount == 0 && redrawStats_.imageEraseCount == 0 && redrawStats_.imagePaintCount == 0
      && redrawStats_.rectSurfaceEraseCount == 0 && redrawStats_.rectSurfacePaintCount == 0)
  {
    return;
  }

  char buffer[512];
  ::snprintf(buffer,
             sizeof(buffer),
             "[win32-redraw] onChange=%d localApply=%d changeFlags=0x%X changeNeedsLayout=%d changeFullRebuild=%d "
             "full=%d rect=%d layoutBounds=%d paintBounds=%d noBounds=%d comp=%d opaque=%d generic=%d root(e=%d p=%d) "
             "cell(e=%d p=%d) image(e=%d p=%d) rect(e=%d p=%d)\n",
             redrawStats_.onChangeCalls,
             redrawStats_.onBoundaryApplyCalls,
             static_cast<unsigned int>(redrawStats_.lastOnChangeFlags),
             redrawStats_.lastOnChangeRequiredLayout ? 1 : 0,
             redrawStats_.lastOnChangeFullRebuild ? 1 : 0,
             redrawStats_.queuedFullWindowInvalidates,
             redrawStats_.queuedRectInvalidates,
             redrawStats_.queuedLayoutBoundsInvalidates,
             redrawStats_.queuedPaintBoundsInvalidates,
             redrawStats_.queuedMissingBoundsInvalidates,
             redrawStats_.queuedCompositedInvalidates,
             redrawStats_.queuedOpaquePaintInvalidates,
             redrawStats_.queuedGenericPaintInvalidates,
             redrawStats_.rootEraseCount,
             redrawStats_.rootPaintCount,
             redrawStats_.cellEraseCount,
             redrawStats_.cellPaintCount,
             redrawStats_.imageEraseCount,
             redrawStats_.imagePaintCount,
             redrawStats_.rectSurfaceEraseCount,
             redrawStats_.rectSurfacePaintCount);
  OutputDebugStringA(buffer);
#endif
}

bool Win32ScenePlatformController::handleCommand(WPARAM wParam, LPARAM lParam)
{
  HWND target = reinterpret_cast<HWND>(lParam);
  WORD code = HIWORD(wParam);
  if (code == BN_CLICKED)
  {
    Win32ButtonContext *button = reinterpret_cast<Win32ButtonContext *>(GetWindowLongPtr(target, GWLP_USERDATA));
    if (!button)
    {
      return false;
    }
    return button->handleCommand(wParam, lParam);
  }
  if (code == EN_CHANGE)
  {
    Win32EditTextContext *edit = reinterpret_cast<Win32EditTextContext *>(GetWindowLongPtr(target, GWLP_USERDATA));
    if (!edit)
    {
      return false;
    }
    return edit->handleCommand(wParam, lParam);
  }
  if (code == CBN_SELCHANGE)
  {
    Win32PopupMenuContext *popup = reinterpret_cast<Win32PopupMenuContext *>(GetWindowLongPtr(target, GWLP_USERDATA));
    if (!popup)
    {
      return false;
    }
    return popup->handleCommand(wParam, lParam);
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
  performLayout(clientWidth_, clientHeight_, false);
}

void Win32ScenePlatformController::performLayout(int clientWidth, int clientHeight, bool rebuildContexts)
{
  pendingInvalidations_.clear();
  if (rebuildContexts)
  {
    clearNodeContexts(rootNode_);
  }
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
}

int Win32ScenePlatformController::applyBoundaryLayoutResult(loka::app::scene::BoundaryNode *boundary,
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

Win32ScenePlatformController::LayoutNodeResult
Win32ScenePlatformController::layoutRectSurfaceNode(loka::app::RectSurfaceNode *surface, const LayoutState &state)
{
  Win32RectSurfaceContext *ctx = static_cast<Win32RectSurfaceContext *>(surface->getContext());
  if (ctx)
  {
    ctx->relayout(state.x, state.y, surface->props.width_, surface->props.height_);
  }
  else
  {
    ctx = new Win32RectSurfaceContext(
        rootHwnd_, state.x, state.y, surface->props.width_, surface->props.height_, surface);
    surface->setContext(ctx);
    ctx->readLifecycleFactOnAttach();
  }
  return LayoutNodeResult(state.width, state.y + surface->props.height_ + kVerticalSpacing);
}

int Win32ScenePlatformController::layoutNode(loka::app::scene::Node *node, const LayoutState &state)
{
  if (!node)
  {
    return state.y;
  }
  return this->applyBoundaryLayoutResult(node->asBoundary(), state.x, state.y, this->computeLayoutResult(node, state));
}

Win32ScenePlatformController::LayoutNodeResult
Win32ScenePlatformController::computeLayoutResult(loka::app::scene::Node *node, const LayoutState &state)
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
      loka::app::scene::Win32PlatformLayoutTraversal traversal(this);
      currentY = handler->layoutNode(column, handlerState, &traversal);
    }
    else
    {
      currentY = loka::app::layout::computeColumnLayoutResultY(
          column, state, this, &Win32ScenePlatformController::layoutContainerChild);
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
      loka::app::scene::Win32PlatformLayoutTraversal traversal(this);
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
      maxY = loka::app::layout::computeRowLayoutResultY(
          row, state, metrics, this, &Win32ScenePlatformController::layoutContainerChild);
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
      loka::app::scene::Win32PlatformLayoutTraversal traversal(this);
      maxY = handler->layoutNode(grid, handlerState, &traversal);
    }
    else
    {
      loka::app::layout::GridLayoutMetrics metrics;
      metrics.gapX = 0;
      metrics.gapY = 0;
      maxY = loka::app::layout::computeGridLayoutResultY(
          grid, state, metrics, this, &Win32ScenePlatformController::layoutContainerChild);
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
      loka::app::scene::Win32PlatformLayoutTraversal traversal(this);
      resultY = handler->layoutNode(box, handlerState, &traversal);
    }
    else
    {
      resultY = loka::app::layout::computeBoxLayoutResultY(
          box, state, this, &Win32ScenePlatformController::layoutContainerChild);
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
      loka::app::scene::Win32PlatformLayoutTraversal traversal(this);
      maxY = handler->layoutNode(stack, handlerState, &traversal);
    }
    else
    {
      maxY = loka::app::layout::computeZStackLayoutResultY(
          stack, state, this, &Win32ScenePlatformController::layoutContainerChild);
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

int Win32ScenePlatformController::layoutContainerChild(void *context,
                                                       loka::app::scene::Node *child,
                                                       const LayoutState &state)
{
  Win32ScenePlatformController *controller = static_cast<Win32ScenePlatformController *>(context);
  if (!controller)
  {
    return state.y;
  }
  return controller->layoutNode(child, state);
}

void Win32ScenePlatformController::clearContexts()
{
  clearNodeContexts(rootNode_);
}

void Win32ScenePlatformController::clearNodeContexts(loka::app::scene::Node *node)
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
