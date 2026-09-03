#include "Win32ScenePlatformController.hpp"
#include "Win32BuiltInSupport.hpp"
#include "app/scene/boundary/Boundary.hpp"
#include <cassert>
#include <climits>
#include <windows.h>
#include <cstdio>
#include <map>
#include <vector>
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/nestable/Grid.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/ScrollView.hpp"
#include "app/nodes/nestable/ZStack.hpp"
#include "app/RectSurface.hpp"
#include "app/layout/FallbackControlMetrics.hpp"
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
#include "context/Win32ScrollViewContext.hpp"

namespace
{
  typedef std::map<HWND, Win32ScenePlatformController *> Win32ControllerMap;
  Win32ControllerMap gControllersByRootHwnd;

  BOOL CALLBACK ApplyDisplayFont(HWND hwnd, LPARAM fontValue)
  {
    SendMessageW(hwnd, WM_SETFONT, static_cast<WPARAM>(fontValue), TRUE);
    return TRUE;
  }

} // namespace

/** Stack owner for one native layout presentation. Contexts keep their usual
    relayout doors, while the controller borrows the outermost pass during the
    synchronous traversal. Reentrant layout scopes join that pass. The borrow
    is removed before RedrawWindow can re-enter native message handling. */
class Win32NativeLayoutPass
{
public:
  explicit Win32NativeLayoutPass(Win32ScenePlatformController *controller)
      : controller_(controller)
  {
    assert(this->controller_);
    if (!this->controller_->activeNativeLayoutPass_)
    {
      this->controller_->activeNativeLayoutPass_ = this;
    }
  }

  ~Win32NativeLayoutPass()
  {
    assert(this->controller_);
    if (this->controller_->activeNativeLayoutPass_ != this)
    {
      assert(this->controller_->activeNativeLayoutPass_
             && "a nested Win32 native layout pass must share an outer owner");
      return;
    }
    this->controller_->activeNativeLayoutPass_ = 0;
    this->controller_->rectSurfaceExtentLedger_.flush();
    if (this->controller_->rootHwnd_)
    {
      RedrawWindow(this->controller_->rootHwnd_,
                   NULL,
                   NULL,
                   RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    }
  }

  void positionNativeWindow(HWND hwnd, int x, int y, int width, int height)
  {
    RECT nativeRect;
    this->controller_->displayScale_.projectFrame(
        loka::core::Frame(x, y, width, height), nativeRect);
    MoveWindow(hwnd,
               nativeRect.left,
               nativeRect.top,
               nativeRect.right - nativeRect.left,
               nativeRect.bottom - nativeRect.top,
               FALSE);
  }

private:
  Win32ScenePlatformController *controller_;

  Win32NativeLayoutPass(const Win32NativeLayoutPass &);
  Win32NativeLayoutPass &operator=(const Win32NativeLayoutPass &);
};

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
        Win32ScenePlatformController *controller_;
        short layoutResultY_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

Win32ScenePlatformController::Win32ScenePlatformController(
    HWND rootHwnd,
    const loka::win32::Win32DisplayScale &displayScale)
    : rootHwnd_(rootHwnd),
      activeNativeLayoutPass_(0),
      rectSurfaceExtentLedger_(),
      projectionParentScopes_(rootHwnd),
      rootNode_(0),
      clientWidth_(0),
      clientHeight_(0),
      displayScale_(displayScale),
      displayFont_()
{
  RegisterWin32BuiltInSupport(*this);
  this->displayFont_.create(this->displayScale_);
  if (rootHwnd_)
  {
    gControllersByRootHwnd[rootHwnd_] = this;
  }
}

Win32ScenePlatformController::~Win32ScenePlatformController()
{
  assert(!this->activeNativeLayoutPass_
         && "a Win32 controller must outlive its stack-owned native layout pass");
  if (rootHwnd_)
  {
    Win32ControllerMap::iterator it = gControllersByRootHwnd.find(rootHwnd_);
    if (it != gControllersByRootHwnd.end() && it->second == this)
    {
      gControllersByRootHwnd.erase(it);
    }
  }
  clearContexts();
  this->drainNativeRetirements();
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
  if (it->second->activeNativeLayoutPass_)
  {
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
    handlerState.height = static_cast<short>(loka::app::layout::FallbackControlMetrics::kTextHeight);
  }
  loka::app::scene::IPlatformNodeHandler *handler = this->nodeHandlerRegistry_.find(node);
  if (!handler)
  {
    assert(
        false
        && "no node handler registered for this node type -- register the handler or an explicit RefusedNodeHandler");
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
  if (it->second->activeNativeLayoutPass_)
  {
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
  HWND root = GetAncestor(targetHwnd, GA_ROOT);
  if (!root)
  {
    root = targetHwnd;
  }
  Win32ControllerMap::iterator it = gControllersByRootHwnd.find(root);
  if (it != gControllersByRootHwnd.end() && it->second
      && it->second->activeNativeLayoutPass_)
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
  (void)fullRebuild;
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
    clientWidth_ = this->displayScale_.unprojectLength(rc.right - rc.left);
    clientHeight_ = this->displayScale_.unprojectLength(rc.bottom - rc.top);
  }
  performLayout(clientWidth_, clientHeight_);
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
  this->displayScale_.projectFrame(
      loka::core::Frame(info.bounds->x,
                        info.bounds->y,
                        info.bounds->width,
                        info.bounds->height),
      rect);
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
  return !pendingInvalidations_.empty() || !retiredWindows_.empty();
}

void Win32ScenePlatformController::queueNativeRetirement(HWND hwnd)
{
  if (hwnd)
  {
    this->retiredWindows_.push_back(hwnd);
  }
}

void Win32ScenePlatformController::drainNativeRetirements()
{
  for (size_t i = 0; i < this->retiredWindows_.size(); ++i)
  {
    if (this->retiredWindows_[i])
    {
      DestroyWindow(this->retiredWindows_[i]);
    }
  }
  this->retiredWindows_.clear();
}

void Win32ScenePlatformController::destroy()
{
  pendingInvalidations_.clear();
  clearContexts();
  this->drainNativeRetirements();
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
      clientWidth = this->displayScale_.unprojectLength(rc.right - rc.left);
      clientHeight = this->displayScale_.unprojectLength(rc.bottom - rc.top);
    }
  }
  clientWidth_ = clientWidth;
  clientHeight_ = clientHeight;
  performLayout(clientWidth_, clientHeight_);
}

void Win32ScenePlatformController::relayoutNativeClientPixels(int clientWidth,
                                                              int clientHeight)
{
  this->relayout(this->displayScale_.unprojectLength(clientWidth),
                 this->displayScale_.unprojectLength(clientHeight));
}

void Win32ScenePlatformController::updateDisplayScale(
    const loka::win32::Win32DisplayScale &displayScale)
{
  this->displayScale_ = displayScale;
  this->ensureDisplayFont();
}

void Win32ScenePlatformController::ensureDisplayFont()
{
  if (this->displayFont_.matches(this->displayScale_))
  {
    return;
  }
  loka::win32::Win32DisplayFont replacement;
  if (replacement.create(this->displayScale_))
  {
    this->applyDisplayFontToNativeSubtree(replacement.get());
    this->displayFont_.swap(replacement);
  }
}

void Win32ScenePlatformController::applyDisplayFontToNativeSubtree(HFONT font)
{
  if (!this->rootHwnd_ || !font)
  {
    return;
  }
  EnumChildWindows(this->rootHwnd_,
                   &ApplyDisplayFont,
                   reinterpret_cast<LPARAM>(font));
}

void Win32ScenePlatformController::positionNativeWindow(HWND hwnd,
                                                        int x,
                                                        int y,
                                                        int width,
                                                        int height)
{
  if (!hwnd)
  {
    return;
  }
  if (this->activeNativeLayoutPass_)
  {
    this->activeNativeLayoutPass_->positionNativeWindow(hwnd, x, y, width, height);
    return;
  }
  RECT nativeRect;
  this->displayScale_.projectFrame(
      loka::core::Frame(x, y, width, height), nativeRect);
  MoveWindow(hwnd,
             nativeRect.left,
             nativeRect.top,
             nativeRect.right - nativeRect.left,
             nativeRect.bottom - nativeRect.top,
             TRUE);
}

HWND Win32ScenePlatformController::createNativeChildWindow(DWORD exStyle,
                                                            LPCWSTR className,
                                                            LPCWSTR windowName,
                                                            DWORD style,
                                                            int x,
                                                            int y,
                                                            int width,
                                                            int height,
                                                            HWND parent,
                                                            HMENU menu,
                                                            HINSTANCE instance,
                                                            void *createParameter)
{
  RECT nativeRect;
  this->displayScale_.projectFrame(
      loka::core::Frame(x, y, width, height), nativeRect);
  HWND hwnd = CreateWindowExW(exStyle,
                              className,
                              windowName,
                              style,
                              nativeRect.left,
                              nativeRect.top,
                              nativeRect.right - nativeRect.left,
                              nativeRect.bottom - nativeRect.top,
                              parent,
                              menu,
                              instance,
                              createParameter);
  if (hwnd && this->displayFont_.get())
  {
    SendMessageW(hwnd,
                 WM_SETFONT,
                 reinterpret_cast<WPARAM>(this->displayFont_.get()),
                 FALSE);
  }
  return hwnd;
}

void Win32ScenePlatformController::performLayout(int clientWidth, int clientHeight)
{
  // A failed DPI-derived font allocation leaves the owned font and its scale
  // paired, so every later layout remains a safe retry point.
  this->ensureDisplayFont();
  pendingInvalidations_.clear();
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
  assert(this->projectionParentScopes_.activeDepth() == 0 &&
         "a Win32 projection pass must begin at the root scope");
  const loka::core::Frame rootClip(0, 0, clientWidth, clientHeight);
  if (!this->projectionParentScopes_.resetRoot(this->rootHwnd_, rootClip))
  {
    return;
  }
  {
    Win32NativeLayoutPass nativeLayoutPass(this);
    PROFILE_SECTION("layout");
    this->layoutNode(this->rootNode_, state);
    assert(this->projectionParentScopes_.activeDepth() == 0 &&
           "a Win32 projection pass must restore the root scope");
  }
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
  loka::app::scene::LayoutState projectedState;
  if (!this->narrowLayoutState(state, projectedState, true))
  {
    return LayoutNodeResult(state.width, state.y);
  }
  const int width = surface->props.width_ > 0 ? surface->props.width_ : state.width;
  const int height = surface->props.height_ > 0 ? surface->props.height_ : state.height;
  this->rectSurfaceExtentLedger_.record(
      surface, loka::core::Frame(state.x, state.y, width, height));
  Win32RectSurfaceContext *ctx = static_cast<Win32RectSurfaceContext *>(surface->getContext());
  if (ctx)
  {
    ctx->relayout(projectedState.x,
                  projectedState.y,
                  width,
                  height);
  }
  else
  {
    ctx = new Win32RectSurfaceContext(
        this,
        this->projectionParentHwnd(),
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

Win32ScenePlatformController::LayoutNodeResult
Win32ScenePlatformController::layoutScrollViewNode(loka::app::ScrollViewNode *scrollView,
                                                   const LayoutState &state)
{
  if (!scrollView || this->projectionParentScopes_.activeDepth() != 0)
  {
    // V1 admits one active viewport. Refuse before creating the inner HWND so
    // the outer scope remains the only structural clipping parent.
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

  Win32ScrollViewContext *ctx =
      static_cast<Win32ScrollViewContext *>(scrollView->getContext());
  if (ctx)
  {
    ctx->relayout(state.x, state.y, state.width, state.height);
  }
  else
  {
    ctx = new Win32ScrollViewContext(this,
                                     this->projectionParentHwnd(),
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
    // A fact far beyond the projected range must not push every child out of
    // range and wedge the layout in a refusal loop before setScrollMetrics
    // (the only precise clamp) can run; the exact maximum is republished
    // after measurement.
    requestedOffset = SHRT_MAX;
  }
  const loka::core::Frame viewportClip(0, 0, state.width, state.height);
  loka::app::scene::ProjectionParentScope viewportScope;
  const loka::app::scene::ProjectionParentScope &parentScope =
      this->projectionParentScopes_.current();
  if (!parentScope.deriveScrolled(
          static_cast<void *>(ctx->hwnd()),
          state.x,
          state.y,
          viewportClip,
          viewportScope))
  {
    return LayoutNodeResult(state.width, state.y);
  }
  loka::app::scene::ProjectionParentScope childScope;
  if (!viewportScope.deriveScrolled(viewportScope.nativeParent,
                                    0,
                                    requestedOffset,
                                    viewportClip,
                                    childScope))
  {
    return LayoutNodeResult(state.width, state.y);
  }

  LayoutState childBase = state;
  {
    RECT viewportClient;
    if (GetClientRect(ctx->hwnd(), &viewportClient))
    {
      // The visible scrollbar occupies non-client width, so the viewport's
      // client width is smaller than the seat width; children lay out
      // against the client, or they extend underneath the scrollbar.
      childBase.width = this->displayScale_.unprojectLength(
          viewportClient.right - viewportClient.left);
    }
  }

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
      // Publish through the complete NodeState door after the scope has
      // popped. A dirty notification may schedule/re-enter projection, and
      // every projection pass must start from the root frame.
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

int Win32ScenePlatformController::layoutNode(loka::app::scene::Node *node, const LayoutState &state)
{
  if (!node)
  {
    return state.y;
  }
  if (this->projectionParentScopes_.activeDepth() != 0 &&
      this->projectionParentScopes_.current().hasShortRangeRefusal())
  {
    // A container above may already have narrowed its next coordinate. Once
    // this frame refuses, nothing else in the subtree may materialize from
    // that poisoned value.
    return state.y;
  }
  return this->applyBoundaryLayoutResult(node->asBoundary(), state.x, state.y, this->computeLayoutResult(node, state));
}

Win32ScenePlatformController::LayoutNodeResult
Win32ScenePlatformController::computeLayoutResult(loka::app::scene::Node *node, const LayoutState &state)
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
      loka::app::scene::Win32PlatformLayoutTraversal traversal(this);
      resultY = handler->layoutNode(stack, handlerState, &traversal);
    }
    else if (stack->props.axis_ == loka::app::STACK_AXIS_COLUMN)
    {
      resultY = loka::app::layout::computeColumnLayoutResultY(
          stack, state, this, &Win32ScenePlatformController::layoutContainerChild);
    }
    else
    {
      const loka::app::layout::RowLayoutMetrics metrics =
          loka::app::layout::FallbackControlMetrics::rowLayout();
      resultY = loka::app::layout::computeRowLayoutResultY(
          stack, state, metrics, this, &Win32ScenePlatformController::layoutContainerChild);
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
      if (!this->narrowLayoutState(state, handlerState, false))
      {
        return LayoutNodeResult(state.width, state.y);
      }
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
      if (!this->narrowLayoutState(state, handlerState, false))
      {
        return LayoutNodeResult(state.width, state.y);
      }
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

Win32ScenePlatformController::LayoutNodeResult
Win32ScenePlatformController::DispatchProjectedLayout(
    Win32ScenePlatformController *controller,
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

bool Win32ScenePlatformController::narrowLayoutState(
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

void Win32ScenePlatformController::refuseScrollViewShortRange()
{
  if (this->projectionParentScopes_.activeDepth() != 0)
  {
    this->projectionParentScopes_.current().markShortRangeRefused();
  }
}

bool Win32ScenePlatformController::refuseNarrowingInScrollScope(int resultY)
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
    return this->displayScale_.unprojectLength(rc.right - rc.left);
  }
  return 260;
}
