#include "MacScenePlatformController.hpp"
#include "MacPlatformNodeHandlers.hpp"
#include "MacPlatformLeafLayoutHandlers.hpp"
#include "MacPlatformHostActionLayoutHandlers.hpp"
#include "app/scene/node/Boundary.hpp"
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
#include "app/ImageView.hpp"
#include "app/RectSurface.hpp"
#include "app/layout/ContainerLayout.hpp"
#include "app/layout/LayoutHeuristics.hpp"
#include "app/layout/PlatformBuiltinLayoutHandlers.hpp"
#include "app/scene/Node.hpp"
#include "context/MacButtonContext.hpp"
#include "context/MacCellContext.hpp"
#include "context/MacEditTextContext.hpp"
#include "context/MacOpenFileDialogContext.hpp"
#include "context/MacTextContext.hpp"
#include "context/MacPopupMenuContext.hpp"
#include "context/MacImageViewContext.hpp"
#include "context/MacRectSurfaceContext.hpp"
#include "loka/core/Profiler.hpp"
#include "loka/platform/StringUTF8.hpp"
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

  int MeasureTextHeightForWidth(const loka::app::TextNode *text,
                                int width,
                                int defaultHeight)
  {
    if (!text || !text->props.text_)
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

    NSString *string = [NSString stringWithUTF8String:utf8.c_str()];
    if (!string)
    {
      return defaultHeight;
    }
    NSDictionary *attrs = [NSDictionary dictionaryWithObject:[NSFont systemFontOfSize:[NSFont systemFontSize]]
                                                      forKey:NSFontAttributeName];
    NSRect rect = [string boundingRectWithSize:NSMakeSize(static_cast<CGFloat>(width), CGFLOAT_MAX)
                                       options:(NSStringDrawingUsesLineFragmentOrigin | NSStringDrawingUsesFontLeading)
                                    attributes:attrs];
    const int measured = static_cast<int>(rect.size.height + 0.5f);
    const int measuredWithPadding = measured + 2;
    if (measuredWithPadding > defaultHeight)
    {
      return measuredWithPadding;
    }
    return defaultHeight;
  }

  int EnsureMacTextLayoutResult(MacScenePlatformController *controller,
                                MacNodeContextMapper &mapper,
                                loka::app::scene::PlatformNodeHandlerRegistry &registry,
                                loka::app::TextNode *text,
                                int x,
                                int y,
                                int width)
  {
    const int textHeight = MeasureTextHeightForWidth(text, width, kTextHeight);
    loka::app::scene::LayoutState handlerState;
    handlerState.x = static_cast<short>(x);
    handlerState.y = static_cast<short>(y);
    handlerState.width = static_cast<short>(width);
    handlerState.height = static_cast<short>(textHeight);
    loka::app::scene::IPlatformNodeHandler *handler = registry.find(text);
    MacTextContext *ctx = 0;
    if (handler)
    {
      ctx = static_cast<MacTextContext *>(handler->ensureContext(text, controller, handlerState));
    }
    if (!ctx)
    {
      ctx = mapper.ensureTextContext(text, x, y, width, textHeight);
    }
    return y + textHeight + kVerticalSpacing;
  }

  int EnsureMacImageViewLayoutResult(MacScenePlatformController *controller,
                                     MacNodeContextMapper &mapper,
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
    MacImageViewContext *ctx = 0;
    if (handler)
    {
      ctx = static_cast<MacImageViewContext *>(handler->ensureContext(image, controller, handlerState));
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
  RegisterMacPlatformLeafLayoutHandlers(*this);
  RegisterMacPlatformHostActionLayoutHandlers(*this);
  RegisterMacPlatformNodeHandlers(this->nodeHandlerRegistry_);
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

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::layoutOpenFileDialogNode(
    loka::app::OpenFileDialogNode *dialog,
    const LayoutState &state)
{
  loka::app::scene::LayoutState handlerState;
  handlerState.x = 0;
  handlerState.y = 0;
  handlerState.width = 0;
  handlerState.height = 0;
  loka::app::scene::IPlatformNodeHandler *handler = this->nodeHandlerRegistry_.find(dialog);
  MacOpenFileDialogContext *ctx = 0;
  if (handler)
  {
    ctx = static_cast<MacOpenFileDialogContext *>(handler->ensureContext(dialog, this, handlerState));
  }
  if (!ctx)
  {
    ctx = this->contextMapper_.ensureOpenFileDialogContext(dialog);
  }
  return LayoutNodeResult(state.width, state.y);
}

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::layoutButtonNode(
    loka::app::ButtonNode *button,
    const LayoutState &state)
{
  MacButtonContext *ctx = this->contextMapper_.ensureButtonContext(button,
                                                                   state.x,
                                                                   state.y,
                                                                   state.width,
                                                                   kButtonHeight);
  return LayoutNodeResult(state.width, state.y + kButtonHeight + kVerticalSpacing);
}

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::layoutEditTextNode(
    loka::app::EditTextNode *edit,
    const LayoutState &state)
{
  loka::app::scene::LayoutState handlerState;
  handlerState.x = static_cast<short>(state.x);
  handlerState.y = static_cast<short>(state.y);
  handlerState.width = static_cast<short>(state.width);
  handlerState.height = static_cast<short>(kEditTextHeight);
  loka::app::scene::IPlatformNodeHandler *handler = this->nodeHandlerRegistry_.find(edit);
  MacEditTextContext *ctx = 0;
  if (handler)
  {
    ctx = static_cast<MacEditTextContext *>(handler->ensureContext(edit, this, handlerState));
  }
  if (!ctx)
  {
    ctx = this->contextMapper_.ensureEditTextContext(edit, state.x, state.y, state.width, kEditTextHeight);
  }
  this->registerEditField(ctx->nativeField());
  return LayoutNodeResult(state.width, state.y + kEditTextHeight + kVerticalSpacing);
}

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::layoutPopupMenuNode(
    loka::app::PopupMenuNode *popup,
    const LayoutState &state)
{
  loka::app::scene::LayoutState handlerState;
  handlerState.x = static_cast<short>(state.x);
  handlerState.y = static_cast<short>(state.y);
  handlerState.width = static_cast<short>(state.width);
  handlerState.height = static_cast<short>(kPopupMenuHeight);
  loka::app::scene::IPlatformNodeHandler *handler = this->nodeHandlerRegistry_.find(popup);
  MacPopupMenuContext *ctx = 0;
  if (handler)
  {
    ctx = static_cast<MacPopupMenuContext *>(handler->ensureContext(popup, this, handlerState));
  }
  if (!ctx)
  {
    ctx = this->contextMapper_.ensurePopupMenuContext(popup, state.x, state.y, state.width, kPopupMenuHeight);
  }
  return LayoutNodeResult(state.width, state.y + kPopupMenuHeight + kVerticalSpacing);
}

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::layoutCellNode(
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
  MacCellContext *ctx = 0;
  if (handler)
  {
    ctx = static_cast<MacCellContext *>(handler->ensureContext(cell, this, handlerState));
  }
  if (!ctx)
  {
    ctx = this->contextMapper_.ensureCellContext(cell, state.x, state.y, state.width, cellHeight);
  }
  return LayoutNodeResult(state.width, state.y + cellHeight);
}

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::layoutTextNode(
    loka::app::TextNode *text,
    const LayoutState &state)
{
  return LayoutNodeResult(
      state.width,
      EnsureMacTextLayoutResult(this, this->contextMapper_, this->nodeHandlerRegistry_, text, state.x, state.y, state.width));
}

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::layoutImageViewNode(
    loka::app::ImageViewNode *image,
    const LayoutState &state)
{
  return LayoutNodeResult(
      state.width,
      EnsureMacImageViewLayoutResult(
          this,
          this->contextMapper_,
          this->nodeHandlerRegistry_,
          image,
          state.x,
          state.y,
          state.width,
          state.height));
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

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::dispatchTextLayout(
    MacScenePlatformController *controller,
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

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::dispatchImageViewLayout(
    MacScenePlatformController *controller,
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

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::dispatchButtonLayout(
    MacScenePlatformController *controller,
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

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::dispatchEditTextLayout(
    MacScenePlatformController *controller,
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

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::dispatchPopupMenuLayout(
    MacScenePlatformController *controller,
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

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::dispatchCellLayout(
    MacScenePlatformController *controller,
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

MacScenePlatformController::LayoutNodeResult MacScenePlatformController::dispatchOpenFileDialogLayout(
    MacScenePlatformController *controller,
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
