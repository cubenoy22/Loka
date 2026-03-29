#include "Win32ScenePlatformController.hpp"
#include "Win32PlatformNodeHandlers.hpp"
#include "Win32PlatformLeafLayoutHandlers.hpp"
#include "Win32PlatformHostActionLayoutHandlers.hpp"
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
#include "app/ImageView.hpp"
#include "app/RectSurface.hpp"
#include "app/layout/ContainerLayout.hpp"
#include "app/layout/LayoutHeuristics.hpp"
#include "app/layout/PlatformBuiltinLayoutHandlers.hpp"
#include "app/scene/Node.hpp"
#include "loka/core/Profiler.hpp"
#include "loka/platform/StringUTF8.hpp"
#include "context/Win32CellContext.hpp"
#include "context/Win32ImageViewContext.hpp"
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

  int MeasureTextHeightForWidth(HWND hwnd,
                                const loka::app::TextNode *text,
                                int width,
                                int defaultHeight)
  {
    if (!hwnd || !text || !text->props.text_)
    {
      return defaultHeight;
    }
    if (!text->props.hasAttr_ || !text->props.attr_.hasWrapValue_ ||
        text->props.attr_.wrapValue_ == loka::app::TEXT_WRAP_NONE)
    {
      return defaultHeight;
    }
    if (width <= 0)
    {
      return defaultHeight;
    }

    std::string utf8;
    if (!loka::platform::CollectUtf8(text->props.text_->get(), utf8))
    {
      return defaultHeight;
    }
    if (utf8.empty())
    {
      return defaultHeight;
    }

    HDC hdc = GetDC(hwnd);
    if (!hdc)
    {
      return defaultHeight;
    }
    RECT rc;
    rc.left = 0;
    rc.top = 0;
    rc.right = width;
    rc.bottom = 0;
    UINT flags = DT_LEFT | DT_NOPREFIX | DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL;
    DrawTextA(hdc, utf8.c_str(), -1, &rc, flags);
    ReleaseDC(hwnd, hdc);

    const int measured = rc.bottom - rc.top;
    const int measuredWithPadding = measured + 8;
    if (measuredWithPadding > defaultHeight)
    {
      return measuredWithPadding;
    }
    return defaultHeight;
  }

  int EnsureWin32TextLayoutResult(HWND rootHwnd,
                                  Win32ScenePlatformController *controller,
                                  Win32NodeContextMapper &mapper,
                                  loka::app::scene::PlatformNodeHandlerRegistry &registry,
                                  loka::app::TextNode *text,
                                  int x,
                                  int y,
                                  int width)
  {
    const int textHeight = MeasureTextHeightForWidth(rootHwnd, text, width, kTextHeight);
    loka::app::scene::LayoutState handlerState;
    handlerState.x = static_cast<short>(x);
    handlerState.y = static_cast<short>(y);
    handlerState.width = static_cast<short>(width);
    handlerState.height = static_cast<short>(textHeight);
    loka::app::scene::IPlatformNodeHandler *handler = registry.find(text);
    Win32TextContext *ctx = 0;
    if (handler)
    {
      ctx = static_cast<Win32TextContext *>(handler->ensureContext(text, controller, handlerState));
    }
    if (!ctx)
    {
      ctx = mapper.ensureTextContext(text, x, y, width, textHeight);
    }
    return y + textHeight + kVerticalSpacing;
  }

  int EnsureWin32ImageViewLayoutResult(Win32ScenePlatformController *controller,
                                       Win32NodeContextMapper &mapper,
                                       loka::app::scene::PlatformNodeHandlerRegistry &registry,
                                       loka::app::ImageViewNode *image,
                                       int x,
                                       int y,
                                       int width,
                                       int height)
  {
    int sizePolicy = loka::app::IMAGE_VIEW_SIZE_AUTO;
    if (image->props.hasAttr_ && image->props.attr_.hasSizePolicyValue_)
    {
      sizePolicy = static_cast<int>(image->props.attr_.sizePolicyValue_);
    }

    const bool hasExplicitWidth = image->props.width_ > 0;
    const bool hasExplicitHeight = image->props.height_ > 0;
    int imageWidth = hasExplicitWidth ? image->props.width_ : width;
    int imageHeight = image->props.height_;
    int srcWidth = 0;
    int srcHeight = 0;
    if (image->props.image_)
    {
      const loka::core::resource::Image current = image->props.image_->get();
      srcWidth = current.width();
      srcHeight = current.height();
    }

    if (sizePolicy == loka::app::IMAGE_VIEW_SIZE_INTRINSIC && !hasExplicitWidth && srcWidth > 0)
    {
      imageWidth = srcWidth;
    }
    else if (sizePolicy == loka::app::IMAGE_VIEW_SIZE_FILL_PARENT && !hasExplicitWidth)
    {
      imageWidth = width;
    }

    if (!hasExplicitHeight)
    {
      if (sizePolicy == loka::app::IMAGE_VIEW_SIZE_FILL_PARENT && height > 0)
      {
        imageHeight = height;
      }
      else if (srcWidth > 0 && srcHeight > 0 && imageWidth > 0)
      {
        imageHeight = (imageWidth * srcHeight) / srcWidth;
      }
      else if (srcHeight > 0)
      {
        imageHeight = srcHeight;
      }
    }
    if (imageHeight <= 0)
    {
      imageHeight = 160;
    }

    loka::app::scene::LayoutState handlerState;
    handlerState.x = static_cast<short>(x);
    handlerState.y = static_cast<short>(y);
    handlerState.width = static_cast<short>(imageWidth);
    handlerState.height = static_cast<short>(imageHeight);
    loka::app::scene::IPlatformNodeHandler *handler = registry.find(image);
    Win32ImageViewContext *ctx = 0;
    if (handler)
    {
      ctx = static_cast<Win32ImageViewContext *>(handler->ensureContext(image, controller, handlerState));
    }
    if (!ctx)
    {
      ctx = mapper.ensureImageViewContext(image, x, y, imageWidth, imageHeight);
    }
    return y + imageHeight + kVerticalSpacing;
  }
}

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
        Win32ScenePlatformController *controller_;
      };
    }
  }
}

Win32ScenePlatformController::Win32ScenePlatformController(HWND rootHwnd)
    : rootHwnd_(rootHwnd), contextMapper_(rootHwnd), rootNode_(0), clientWidth_(0), clientHeight_(0)
{
  loka::app::layout::RowLayoutMetrics rowMetrics;
  rowMetrics.gap = kHorizontalSpacing;
  rowMetrics.fallbackHeight = kTextHeight;
  rowMetrics.buttonHeight = kButtonHeight;
  rowMetrics.editTextHeight = kEditTextHeight;
  rowMetrics.popupMenuHeight = kPopupMenuHeight;
  rowMetrics.textHeight = kTextHeight;
  rowMetrics.imageFallbackHeight = kImageFallbackHeightModern;
  loka::app::layout::GridLayoutMetrics gridMetrics;
  gridMetrics.gapX = 0;
  gridMetrics.gapY = 0;
  loka::app::layout::RegisterBuiltinPlatformLayoutHandlers(this->layoutHandlerRegistry_, &rowMetrics, &gridMetrics);
  RegisterWin32PlatformLeafLayoutHandlers(*this);
  RegisterWin32PlatformHostActionLayoutHandlers(*this);
  RegisterWin32PlatformNodeHandlers(this->nodeHandlerRegistry_);
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

void Win32ScenePlatformController::onChange(loka::app::scene::Node *rootNode, loka::app::scene::NodeDirtyFlags flags, bool fullRebuild)
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

  const bool requiresLayout = (flags & loka::app::scene::NODE_DIRTY_INITIAL) != 0 ||
                              (flags & loka::app::scene::NODE_DIRTY_LAYOUT) != 0 ||
                              (flags & loka::app::scene::NODE_DIRTY_CHILD) != 0;
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

void Win32ScenePlatformController::queueDirtyRect(HWND targetHwnd, const RECT *rect, BOOL eraseBackground, bool includeChildren)
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
  if (redrawStats_.onChangeCalls == 0 &&
      redrawStats_.onBoundaryApplyCalls == 0 &&
      redrawStats_.queuedFullWindowInvalidates == 0 &&
      redrawStats_.queuedRectInvalidates == 0 &&
      redrawStats_.rootEraseCount == 0 &&
      redrawStats_.rootPaintCount == 0 &&
      redrawStats_.cellEraseCount == 0 &&
      redrawStats_.cellPaintCount == 0 &&
      redrawStats_.imageEraseCount == 0 &&
      redrawStats_.imagePaintCount == 0 &&
      redrawStats_.rectSurfaceEraseCount == 0 &&
      redrawStats_.rectSurfacePaintCount == 0)
  {
    return;
  }

  char buffer[512];
  ::snprintf(buffer,
             sizeof(buffer),
             "[win32-redraw] onChange=%d localApply=%d changeFlags=0x%X changeNeedsLayout=%d changeFullRebuild=%d full=%d rect=%d layoutBounds=%d paintBounds=%d noBounds=%d comp=%d opaque=%d generic=%d root(e=%d p=%d) cell(e=%d p=%d) image(e=%d p=%d) rect(e=%d p=%d)\n",
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
  performLayout(clientWidth_, clientHeight_, false);
}

void Win32ScenePlatformController::performLayout(int clientWidth, int clientHeight, bool rebuildContexts)
{
  pendingInvalidations_.clear();
  buttonMap_.clear();
  editMap_.clear();
  popupMap_.clear();
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

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::layoutOpenFileDialogNode(
    loka::app::OpenFileDialogNode *dialog,
    const LayoutState &state)
{
  loka::app::scene::LayoutState handlerState;
  handlerState.x = 0;
  handlerState.y = 0;
  handlerState.width = 0;
  handlerState.height = 0;
  loka::app::scene::IPlatformNodeHandler *handler = this->nodeHandlerRegistry_.find(dialog);
  Win32OpenFileDialogContext *ctx = 0;
  if (handler)
  {
    ctx = static_cast<Win32OpenFileDialogContext *>(handler->ensureContext(dialog, this, handlerState));
  }
  if (!ctx)
  {
    ctx = this->contextMapper_.ensureOpenFileDialogContext(dialog);
  }
  return LayoutNodeResult(state.width, state.y);
}

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::layoutButtonNode(
    loka::app::ButtonNode *button,
    const LayoutState &state)
{
  Win32ButtonContext *ctx = this->contextMapper_.ensureButtonContext(button,
                                                                     state.x,
                                                                     state.y,
                                                                     state.width,
                                                                     kButtonHeight);
  this->buttonMap_[ctx->hwnd()] = ctx;
  return LayoutNodeResult(state.width, state.y + kButtonHeight + kVerticalSpacing);
}

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::layoutEditTextNode(
    loka::app::EditTextNode *edit,
    const LayoutState &state)
{
  loka::app::scene::LayoutState handlerState;
  handlerState.x = static_cast<short>(state.x);
  handlerState.y = static_cast<short>(state.y);
  handlerState.width = static_cast<short>(state.width);
  handlerState.height = static_cast<short>(kEditTextHeight);
  loka::app::scene::IPlatformNodeHandler *handler = this->nodeHandlerRegistry_.find(edit);
  Win32EditTextContext *ctx = 0;
  if (handler)
  {
    ctx = static_cast<Win32EditTextContext *>(handler->ensureContext(edit, this, handlerState));
  }
  if (!ctx)
  {
    ctx = this->contextMapper_.ensureEditTextContext(edit, state.x, state.y, state.width, kEditTextHeight);
  }
  this->editMap_[ctx->hwnd()] = ctx;
  return LayoutNodeResult(state.width, state.y + kEditTextHeight + kVerticalSpacing);
}

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::layoutPopupMenuNode(
    loka::app::PopupMenuNode *popup,
    const LayoutState &state)
{
  loka::app::scene::LayoutState handlerState;
  handlerState.x = static_cast<short>(state.x);
  handlerState.y = static_cast<short>(state.y);
  handlerState.width = static_cast<short>(state.width);
  handlerState.height = static_cast<short>(kPopupMenuHeight);
  loka::app::scene::IPlatformNodeHandler *handler = this->nodeHandlerRegistry_.find(popup);
  Win32PopupMenuContext *ctx = 0;
  if (handler)
  {
    ctx = static_cast<Win32PopupMenuContext *>(handler->ensureContext(popup, this, handlerState));
  }
  if (!ctx)
  {
    ctx = this->contextMapper_.ensurePopupMenuContext(popup, state.x, state.y, state.width, kPopupMenuHeight);
  }
  this->popupMap_[ctx->hwnd()] = ctx;
  return LayoutNodeResult(state.width, state.y + kPopupMenuHeight + kVerticalSpacing);
}

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::layoutCellNode(
    loka::app::CellNode *cell,
    const LayoutState &state)
{
  const int cellHeight = state.height > 0 ? state.height : kTextHeight;
  loka::app::scene::LayoutState handlerState;
  handlerState.x = static_cast<short>(state.x);
  handlerState.y = static_cast<short>(state.y);
  handlerState.width = static_cast<short>(state.width);
  handlerState.height = static_cast<short>(cellHeight);
  loka::app::scene::IPlatformNodeHandler *handler = this->nodeHandlerRegistry_.find(cell);
  Win32CellContext *ctx = 0;
  if (handler)
  {
    ctx = static_cast<Win32CellContext *>(handler->ensureContext(cell, this, handlerState));
  }
  if (!ctx)
  {
    ctx = this->contextMapper_.ensureCellContext(cell, state.x, state.y, state.width, cellHeight);
  }
  int resultY = state.y + cellHeight;
  if (state.height <= 0)
  {
    resultY += kVerticalSpacing;
  }
  return LayoutNodeResult(state.width, resultY);
}

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::layoutTextNode(
    loka::app::TextNode *text,
    const LayoutState &state)
{
  return LayoutNodeResult(
      state.width,
      EnsureWin32TextLayoutResult(rootHwnd_, this, this->contextMapper_, this->nodeHandlerRegistry_, text, state.x, state.y, state.width));
}

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::layoutImageViewNode(
    loka::app::ImageViewNode *image,
    const LayoutState &state)
{
  return LayoutNodeResult(
      state.width,
      EnsureWin32ImageViewLayoutResult(
          this,
          this->contextMapper_,
          this->nodeHandlerRegistry_,
          image,
          state.x,
          state.y,
          state.width,
          state.height));
}

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::layoutRectSurfaceNode(
    loka::app::RectSurfaceNode *surface,
    const LayoutState &state)
{
  Win32RectSurfaceContext *ctx = static_cast<Win32RectSurfaceContext *>(surface->getContext());
  if (ctx)
  {
    ctx->relayout(state.x, state.y, surface->props.width_, surface->props.height_);
  }
  else
  {
    ctx = new Win32RectSurfaceContext(rootHwnd_,
                                      state.x,
                                      state.y,
                                      surface->props.width_,
                                      surface->props.height_,
                                      surface);
    surface->setContext(ctx);
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

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::computeLayoutResult(
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
      loka::app::scene::Win32PlatformLayoutTraversal traversal(this);
      currentY = handler->layoutNode(column, handlerState, &traversal);
    }
    else
    {
      currentY = loka::app::layout::computeColumnLayoutResultY(column, state, this, &Win32ScenePlatformController::layoutContainerChild);
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
      maxY = loka::app::layout::computeRowLayoutResultY(row, state, metrics, this, &Win32ScenePlatformController::layoutContainerChild);
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
      maxY = loka::app::layout::computeGridLayoutResultY(grid, state, metrics, this, &Win32ScenePlatformController::layoutContainerChild);
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
      resultY = loka::app::layout::computeBoxLayoutResultY(box, state, this, &Win32ScenePlatformController::layoutContainerChild);
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
      maxY = loka::app::layout::computeZStackLayoutResultY(stack, state, this, &Win32ScenePlatformController::layoutContainerChild);
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

int Win32ScenePlatformController::layoutContainerChild(void *context, loka::app::scene::Node *child, const LayoutState &state)
{
  Win32ScenePlatformController *controller = static_cast<Win32ScenePlatformController *>(context);
  if (!controller)
  {
    return state.y;
  }
  return controller->layoutNode(child, state);
}

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::dispatchTextLayout(
    Win32ScenePlatformController *controller,
    loka::app::scene::Node *node,
    const LayoutState &state)
{
  if (!controller || !node)
  {
    return LayoutNodeResult(state.width, state.y);
  }
  loka::app::TextNode *text = node->asTextNode();
  if (!text)
  {
    return LayoutNodeResult(state.width, state.y);
  }
  return controller->layoutTextNode(text, state);
}

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::dispatchImageViewLayout(
    Win32ScenePlatformController *controller,
    loka::app::scene::Node *node,
    const LayoutState &state)
{
  if (!controller || !node)
  {
    return LayoutNodeResult(state.width, state.y);
  }
  loka::app::ImageViewNode *image = node->asImageViewNode();
  if (!image)
  {
    return LayoutNodeResult(state.width, state.y);
  }
  return controller->layoutImageViewNode(image, state);
}

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::dispatchButtonLayout(
    Win32ScenePlatformController *controller,
    loka::app::scene::Node *node,
    const LayoutState &state)
{
  if (!controller || !node)
  {
    return LayoutNodeResult(state.width, state.y);
  }
  loka::app::ButtonNode *button = node->asButtonNode();
  if (!button)
  {
    return LayoutNodeResult(state.width, state.y);
  }
  return controller->layoutButtonNode(button, state);
}

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::dispatchEditTextLayout(
    Win32ScenePlatformController *controller,
    loka::app::scene::Node *node,
    const LayoutState &state)
{
  if (!controller || !node)
  {
    return LayoutNodeResult(state.width, state.y);
  }
  loka::app::EditTextNode *edit = node->asEditTextNode();
  if (!edit)
  {
    return LayoutNodeResult(state.width, state.y);
  }
  return controller->layoutEditTextNode(edit, state);
}

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::dispatchPopupMenuLayout(
    Win32ScenePlatformController *controller,
    loka::app::scene::Node *node,
    const LayoutState &state)
{
  if (!controller || !node)
  {
    return LayoutNodeResult(state.width, state.y);
  }
  loka::app::PopupMenuNode *popup = node->asPopupMenuNode();
  if (!popup)
  {
    return LayoutNodeResult(state.width, state.y);
  }
  return controller->layoutPopupMenuNode(popup, state);
}

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::dispatchCellLayout(
    Win32ScenePlatformController *controller,
    loka::app::scene::Node *node,
    const LayoutState &state)
{
  if (!controller || !node)
  {
    return LayoutNodeResult(state.width, state.y);
  }
  loka::app::CellNode *cell = node->asCellNode();
  if (!cell)
  {
    return LayoutNodeResult(state.width, state.y);
  }
  return controller->layoutCellNode(cell, state);
}

Win32ScenePlatformController::LayoutNodeResult Win32ScenePlatformController::dispatchOpenFileDialogLayout(
    Win32ScenePlatformController *controller,
    loka::app::scene::Node *node,
    const LayoutState &state)
{
  if (!controller || !node)
  {
    return LayoutNodeResult(state.width, state.y);
  }
  loka::app::OpenFileDialogNode *dialog = node->asOpenFileDialogNode();
  if (!dialog)
  {
    return LayoutNodeResult(state.width, state.y);
  }
  return controller->layoutOpenFileDialogNode(dialog, state);
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
