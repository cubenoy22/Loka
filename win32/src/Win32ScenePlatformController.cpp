#include "Win32ScenePlatformController.hpp"
#include "Win32PlatformNodeHandlers.hpp"
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
#include "app/layout/LayoutHeuristics.hpp"
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
}

Win32ScenePlatformController::Win32ScenePlatformController(HWND rootHwnd)
    : rootHwnd_(rootHwnd), contextMapper_(rootHwnd), rootNode_(0), clientWidth_(0), clientHeight_(0)
{
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

  if (loka::app::ColumnNode *column = node->asColumnNode())
  {
    if (column->childrenHead() == 0 || column->childrenCount() == 0)
    {
      return state.y;
    }
    LayoutState childState = state;
    int currentY = state.y;
    loka::dsl::CompositionCursor<loka::app::scene::Node> it(column->childrenHead(), column->childrenCount());
    for (loka::app::scene::Node *child = it.next(); child; child = it.next())
    {
      childState = state;
      childState.y = currentY;
      if (state.height > 0)
      {
        childState.height = loka::app::layout::remainingChildHeightForColumn(state.height, state.y, currentY);
      }
      int childWidth = state.width;
      int childOffset = 0;
      if (column->props.hasHorizontalAlignment_)
      {
        childWidth = loka::app::layout::preferredChildWidthForColumn(child, state.width);
        const int remain = state.width - childWidth;
        if (remain > 0)
        {
          if (column->props.horizontalAlignment_ == loka::app::HORIZONTAL_ALIGNMENT_CENTER)
          {
            childOffset = remain / 2;
          }
          else if (column->props.horizontalAlignment_ == loka::app::HORIZONTAL_ALIGNMENT_TRAILING)
          {
            childOffset = remain;
          }
        }
      }
      childState.x = state.x + childOffset;
      childState.width = childWidth;
      currentY = layoutNode(child, childState);
    }
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, currentY);
  }

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
    int rowHeight = state.height > 0 ? state.height : 0;
    if (row->props.hasVerticalAlignment_)
    {
      rowHeight = 0;
      loka::dsl::CompositionCursor<loka::app::scene::Node> measure(row->childrenHead(), childCount);
      for (loka::app::scene::Node *child = measure.next(); child; child = measure.next())
      {
        const int h = loka::app::layout::preferredChildHeightForRow(child,
                                                                    kTextHeight,
                                                                    kButtonHeight,
                                                                    kEditTextHeight,
                                                                    kPopupMenuHeight,
                                                                    kTextHeight,
                                                                    kImageFallbackHeightModern);
        if (h > rowHeight)
        {
          rowHeight = h;
        }
      }
      if (rowHeight <= 0)
      {
        rowHeight = kTextHeight;
      }
    }
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
      if (row->props.hasVerticalAlignment_)
      {
        const int childHeight = loka::app::layout::preferredChildHeightForRow(child,
                                                                               rowHeight,
                                                                               kButtonHeight,
                                                                               kEditTextHeight,
                                                                               kPopupMenuHeight,
                                                                               kTextHeight,
                                                                               kImageFallbackHeightModern);
        int offset = 0;
        const int remain = rowHeight - childHeight;
        if (remain > 0)
        {
          if (row->props.verticalAlignment_ == loka::app::VERTICAL_ALIGNMENT_CENTER)
          {
            offset = remain / 2;
          }
          else if (row->props.verticalAlignment_ == loka::app::VERTICAL_ALIGNMENT_BOTTOM)
          {
            offset = remain;
          }
        }
        childState.y = state.y + offset;
        childState.height = childHeight;
      }
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
    Win32ButtonContext *ctx = this->contextMapper_.ensureButtonContext(button,
                                                                       state.x,
                                                                       state.y,
                                                                       state.width,
                                                                       kButtonHeight);
    buttonMap_[ctx->hwnd()] = ctx;

    LayoutState nextState = state;
    nextState.y = state.y + kButtonHeight + kVerticalSpacing;
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, nextState.y);
  }

  if (loka::app::EditTextNode *edit = node->asEditTextNode())
  {
    Win32EditTextContext *ctx = static_cast<Win32EditTextContext *>(edit->getContext());
    if (ctx)
    {
      ctx->relayout(state.x, state.y, state.width, kEditTextHeight);
    }
    else
    {
      ctx = new Win32EditTextContext(rootHwnd_, state.x, state.y, state.width, kEditTextHeight, edit);
      edit->setContext(ctx);
    }
    editMap_[ctx->hwnd()] = ctx;

    LayoutState nextState = state;
    nextState.y = state.y + kEditTextHeight + kVerticalSpacing;
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, nextState.y);
  }

  if (loka::app::PopupMenuNode *popup = node->asPopupMenuNode())
  {
    Win32PopupMenuContext *ctx = static_cast<Win32PopupMenuContext *>(popup->getContext());
    if (ctx)
    {
      ctx->relayout(state.x, state.y, state.width, kPopupMenuHeight);
    }
    else
    {
      ctx = new Win32PopupMenuContext(rootHwnd_, state.x, state.y, state.width, kPopupMenuHeight, popup);
      popup->setContext(ctx);
    }
    popupMap_[ctx->hwnd()] = ctx;

    LayoutState nextState = state;
    nextState.y = state.y + kPopupMenuHeight + kVerticalSpacing;
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, nextState.y);
  }

  if (loka::app::CellNode *cell = node->asCellNode())
  {
    const int cellHeight = state.height > 0 ? state.height : kTextHeight;
    Win32CellContext *ctx = static_cast<Win32CellContext *>(cell->getContext());
    if (ctx)
    {
      ctx->relayout(state.x, state.y, state.width, cellHeight);
    }
    else
    {
      ctx = new Win32CellContext(rootHwnd_, state.x, state.y, state.width, cellHeight, cell);
      cell->setContext(ctx);
    }

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
    const int textHeight = MeasureTextHeightForWidth(rootHwnd_, text, state.width, kTextHeight);
    loka::app::scene::LayoutState handlerState;
    handlerState.x = static_cast<short>(state.x);
    handlerState.y = static_cast<short>(state.y);
    handlerState.width = static_cast<short>(state.width);
    handlerState.height = static_cast<short>(textHeight);
    loka::app::scene::IPlatformNodeHandler *handler = this->nodeHandlerRegistry_.find(text);
    Win32TextContext *ctx = 0;
    if (handler)
    {
      ctx = static_cast<Win32TextContext *>(handler->ensureContext(text, this, handlerState));
    }
    if (!ctx)
    {
      ctx = this->contextMapper_.ensureTextContext(text, state.x, state.y, state.width, textHeight);
    }

    LayoutState nextState = state;
    nextState.y = state.y + textHeight + kVerticalSpacing;
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, nextState.y);
  }

  if (loka::app::ImageViewNode *image = node->asImageViewNode())
  {
    int sizePolicy = loka::app::IMAGE_VIEW_SIZE_AUTO;
    if (image->props.hasAttr_ && image->props.attr_.hasSizePolicyValue_)
    {
      sizePolicy = static_cast<int>(image->props.attr_.sizePolicyValue_);
    }

    const bool hasExplicitWidth = image->props.width_ > 0;
    const bool hasExplicitHeight = image->props.height_ > 0;
    int imageWidth = hasExplicitWidth ? image->props.width_ : state.width;
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
      imageWidth = state.width;
    }

    if (!hasExplicitHeight)
    {
      if (sizePolicy == loka::app::IMAGE_VIEW_SIZE_FILL_PARENT && state.height > 0)
      {
        imageHeight = state.height;
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
    handlerState.x = static_cast<short>(state.x);
    handlerState.y = static_cast<short>(state.y);
    handlerState.width = static_cast<short>(imageWidth);
    handlerState.height = static_cast<short>(imageHeight);
    loka::app::scene::IPlatformNodeHandler *handler = this->nodeHandlerRegistry_.find(image);
    Win32ImageViewContext *ctx = 0;
    if (handler)
    {
      ctx = static_cast<Win32ImageViewContext *>(handler->ensureContext(image, this, handlerState));
    }
    if (!ctx)
    {
      ctx = this->contextMapper_.ensureImageViewContext(image, state.x, state.y, imageWidth, imageHeight);
    }

    LayoutState nextState = state;
    nextState.y = state.y + imageHeight + kVerticalSpacing;
    return ApplyBoundaryBounds(boundary, startX, startY, startWidth, nextState.y);
  }

  if (loka::app::RectSurfaceNode *surface = node->asRectSurfaceNode())
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

    LayoutState nextState = state;
    nextState.y = state.y + surface->props.height_ + kVerticalSpacing;
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
