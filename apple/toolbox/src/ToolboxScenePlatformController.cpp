#include "ToolboxScenePlatformController.hpp"
#include "ToolboxWindow.hpp"
#include "ToolboxWindowContext.hpp"
#include "ToolboxProfiler.hpp"
#include "loka/core/Profiler.hpp"
#include <Quickdraw.h>
#include <Controls.h>
#include <cstring>
#include <string>
#include <Memory.h>
#include <Menus.h>

// Profile result captured once on first render
static bool sProfileCaptured = false;
#include "loka/platform/StringUTF8.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"
#include "loka/core/String.hpp"
#include "app/Text.hpp"
#include "app/Button.hpp"
#include "app/Cell.hpp"
#include "app/EditText.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/PopupMenu.hpp"
#include "app/ImageView.hpp"
#include "app/Box.hpp"
#include "app/Grid.hpp"
#include "app/ZStack.hpp"
#include "app/RowColumn.hpp"
#include "app/layout/LayoutHeuristics.hpp"
#include "context/ToolboxNodeContextMapper.hpp"
#include "context/ToolboxPopupMenuContext.hpp"
#include "context/ToolboxButtonContext.hpp"
#include "context/ToolboxCellContext.hpp"
#include "context/ToolboxEditTextContext.hpp"
#include "context/ToolboxTextContext.hpp"
#include "context/ToolboxImageViewContext.hpp"
#include "context/ToolboxLayoutUtil.hpp"
#include "app/scene/Node.hpp"
#include "app/scene/node/Boundary.hpp"

namespace
{
#if !defined(pushButProc)
  enum
  {
    pushButProc = 0
  };
#endif

  static const short kAutoControlBaseId = 128;
  static const short kImageFallbackHeightToolbox = 80;

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

  short ClampToAvailable(short value, short available)
  {
    if (value < 0)
    {
      return 0;
    }
    if (available >= 0 && value > available)
    {
      return available;
    }
    return value;
  }

  short PreferredChildWidthForColumn(loka::app::scene::Node *child, short availableWidth)
  {
    if (!child)
    {
      return ClampToAvailable(availableWidth, availableWidth);
    }
    if (loka::app::ImageViewNode *image = child->asImageViewNode())
    {
      if (image->props.width_ > 0)
      {
        return ClampToAvailable(static_cast<short>(image->props.width_), availableWidth);
      }
    }
    return ClampToAvailable(availableWidth, availableWidth);
  }

  short PreferredChildHeightForRow(loka::app::scene::Node *child, short fallbackHeight)
  {
    if (!child)
    {
      return fallbackHeight;
    }
    if (loka::app::ImageViewNode *image = child->asImageViewNode())
    {
      if (image->props.height_ > 0)
      {
        return static_cast<short>(image->props.height_);
      }
      if (fallbackHeight > 0)
      {
        return fallbackHeight;
      }
      return kImageFallbackHeightToolbox;
    }
    return fallbackHeight;
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

  short LayoutNode(loka::app::scene::Node *node,
                   loka::app::scene::LayoutState &state,
                   ToolboxScenePlatformController *controller,
                   loka::app::scene::BoundaryNode *currentBoundary);
  void RenderNode(loka::app::scene::Node *node,
                  ToolboxScenePlatformController *controller);

  short MaxExplicitControlId(loka::app::scene::Node *node)
  {
    if (!node)
    {
      return 0;
    }
    short maxId = 0;
    if (loka::app::ButtonNode *button = node->asButtonNode())
    {
      short id = button->props.toolboxControlId_;
      if (id <= 0 && button->props.controlTag_ > 0 && button->props.controlTag_ <= 32767)
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
      short id = edit->props.toolboxControlId_;
      if (id <= 0 && edit->props.controlTag_ > 0 && edit->props.controlTag_ <= 32767)
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

  short LayoutChildren(loka::app::scene::INestable *nestable,
                       loka::app::scene::LayoutState &state,
                       ToolboxScenePlatformController *controller,
                       loka::app::scene::BoundaryNode *currentBoundary)
  {
    if (!nestable)
    {
      return 0;
    }
    short maxWidth = 0;
    loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
    for (loka::app::scene::Node *child = it.next(); child; child = it.next())
    {
      short width = LayoutNode(child, state, controller, currentBoundary);
      if (width > maxWidth)
      {
        maxWidth = width;
      }
    }
    return maxWidth;
  }

  short LayoutNode(loka::app::scene::Node *node,
                   loka::app::scene::LayoutState &state,
                   ToolboxScenePlatformController *controller,
                   loka::app::scene::BoundaryNode *currentBoundary)
  {
    if (!node)
    {
      return 0;
    }
    loka::app::scene::BoundaryNode *boundary = node->asBoundary();
    loka::app::scene::BoundaryNode *activeBoundary = boundary ? boundary : currentBoundary;
    const short startX = state.x;
    const short startY = state.y;
    const short startTop = static_cast<short>(startY - state.lineHeight + 2);
    switch (node->kind())
    {
    case loka::app::scene::NODE_KIND_COLUMN:
    {
      loka::app::ColumnNode *column = static_cast<loka::app::ColumnNode *>(node);
      short width = 0;
      short currentY = state.y;
      loka::dsl::CompositionCursor<loka::app::scene::Node> it(column->childrenHead(), column->childrenCount());
      for (loka::app::scene::Node *child = it.next(); child; child = it.next())
      {
        loka::app::scene::LayoutState childState = state;
        childState.y = currentY;
        if (state.height > 0)
        {
          childState.height = static_cast<short>(
              loka::app::layout::remainingChildHeightForColumn(state.height, state.y, currentY));
        }
        short childWidth = state.width;
        short childOffset = 0;
        if (column->props.hasHorizontalAlignment_)
        {
          childWidth = PreferredChildWidthForColumn(child, state.width);
          short remain = static_cast<short>(state.width - childWidth);
          if (remain > 0)
          {
            if (column->props.horizontalAlignment_ == loka::app::HORIZONTAL_ALIGNMENT_CENTER)
            {
              childOffset = static_cast<short>(remain / 2);
            }
            else if (column->props.horizontalAlignment_ == loka::app::HORIZONTAL_ALIGNMENT_TRAILING)
            {
              childOffset = remain;
            }
          }
        }
        childState.x = static_cast<short>(state.x + childOffset);
        childState.width = childWidth;
        short childUsedWidth = LayoutNode(child, childState, controller, activeBoundary);
        if (childUsedWidth > width)
        {
          width = childUsedWidth;
        }
        currentY = childState.y;
      }
      state.y = currentY;
      if (boundary)
      {
        boundary->setLayoutBounds(startX, startTop, width, static_cast<short>(state.y - startTop));
      }
      return width;
    }
    case loka::app::scene::NODE_KIND_BOX:
    {
      loka::app::BoxNode *box = static_cast<loka::app::BoxNode *>(node);
      short padding = static_cast<short>(box->props.padding);
      loka::app::scene::LayoutState childState = state;
      childState.x = static_cast<short>(state.x + padding);
      childState.y = static_cast<short>(state.y + padding);
      if (childState.width > 0)
      {
        childState.width = static_cast<short>(childState.width - padding * 2);
        if (childState.width < 0)
        {
          childState.width = 0;
        }
      }
      if (childState.height > 0)
      {
        childState.height = static_cast<short>(childState.height - padding * 2);
        if (childState.height < 0)
        {
          childState.height = 0;
        }
      }
      short childWidth = LayoutChildren(box->asNestable(), childState, controller, activeBoundary);
      short width = static_cast<short>(childWidth + padding * 2);
      if (childWidth == 0 && box->childrenCount() == 0)
      {
        width = static_cast<short>(padding * 2);
      }
      short resultY = static_cast<short>(childState.y + padding);
      state.y = resultY;
      if (boundary)
      {
        boundary->setLayoutBounds(startX, startTop, width, static_cast<short>(state.y - startTop));
      }
      return width;
    }
    case loka::app::scene::NODE_KIND_ZSTACK:
    {
      loka::app::ZStackNode *stack = static_cast<loka::app::ZStackNode *>(node);
      loka::app::scene::LayoutState childState = state;
      short maxWidth = 0;
      short maxY = state.y;
      if (loka::app::scene::INestable *nestable = stack->asNestable())
      {
        loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
        for (loka::app::scene::Node *child = it.next(); child; child = it.next())
        {
          childState = state;
          short width = LayoutNode(child, childState, controller, activeBoundary);
          if (width > maxWidth)
          {
            maxWidth = width;
          }
          if (childState.y > maxY)
          {
            maxY = childState.y;
          }
        }
      }
      state.y = maxY;
      if (boundary)
      {
        boundary->setLayoutBounds(startX, startTop, maxWidth, static_cast<short>(state.y - startTop));
      }
      return maxWidth;
    }
    case loka::app::scene::NODE_KIND_GRID:
    {
      loka::app::GridNode *grid = static_cast<loka::app::GridNode *>(node);
      const short rows = grid->props.rows > 0 ? grid->props.rows : 1;
      const short cols = grid->props.cols > 0 ? grid->props.cols : 1;
      const short gap = 0;
      short availableWidth = state.width;
      if (availableWidth > 0)
      {
        availableWidth = static_cast<short>(availableWidth - gap * (cols - 1));
        if (availableWidth < 0)
        {
          availableWidth = 0;
        }
      }
      short availableHeight = state.height;
      if (availableHeight > 0)
      {
        availableHeight = static_cast<short>(availableHeight - gap * (rows - 1));
        if (availableHeight < 0)
        {
          availableHeight = 0;
        }
      }
      const short cellWidth = cols > 0 ? static_cast<short>(availableWidth / cols) : 0;
      const short cellHeight = rows > 0 ? static_cast<short>(availableHeight / rows) : 0;
      short maxWidth = static_cast<short>(cellWidth * cols + gap * (cols > 0 ? cols - 1 : 0));
      short maxY = state.y;
      if (loka::app::scene::INestable *nestable = grid->asNestable())
      {
        const size_t childCount = nestable->childrenCount();
        const size_t maxCount = static_cast<size_t>(rows * cols);
        size_t index = 0;
        loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), childCount);
        for (loka::app::scene::Node *child = it.next(); child && index < maxCount; child = it.next(), ++index)
        {
          const short row = static_cast<short>(index / cols);
          const short col = static_cast<short>(index % cols);
          loka::app::scene::LayoutState cellState = state;
          cellState.x = static_cast<short>(state.x + col * (cellWidth + gap));
          cellState.y = static_cast<short>(state.y + row * (cellHeight + gap));
          cellState.width = cellWidth;
          cellState.height = cellHeight;
          short width = LayoutNode(child, cellState, controller, activeBoundary);
          if (width > maxWidth)
          {
            maxWidth = width;
          }
          if (cellState.y > maxY)
          {
            maxY = cellState.y;
          }
        }
      }
      if (cellHeight > 0)
      {
        short totalHeight = static_cast<short>(cellHeight * rows + gap * (rows > 0 ? rows - 1 : 0));
        short bottom = static_cast<short>(state.y + totalHeight);
        if (bottom > maxY)
        {
          maxY = bottom;
        }
      }
      state.y = maxY;
      if (boundary)
      {
        boundary->setLayoutBounds(startX, startTop, maxWidth, static_cast<short>(state.y - startTop));
      }
      return maxWidth;
    }
    case loka::app::scene::NODE_KIND_ROW:
    {
      loka::app::RowNode *row = static_cast<loka::app::RowNode *>(node);
      short rowStartX = state.x;
      short maxHeight = 0;
      short rowHeight = state.lineHeight > 0 ? state.lineHeight : 12;
      if (row->props.hasVerticalAlignment_)
      {
        rowHeight = 0;
        loka::dsl::CompositionCursor<loka::app::scene::Node> measure(row->childrenHead(), row->childrenCount());
        for (loka::app::scene::Node *child = measure.next(); child; child = measure.next())
        {
          short h = PreferredChildHeightForRow(child, state.lineHeight > 0 ? state.lineHeight : 12);
          if (h > rowHeight)
          {
            rowHeight = h;
          }
        }
        if (rowHeight <= 0)
        {
          rowHeight = 12;
        }
      }
      loka::dsl::CompositionCursor<loka::app::scene::Node> it(row->childrenHead(), row->childrenCount());
      for (loka::app::scene::Node *child = it.next(); child; child = it.next())
      {
        loka::app::scene::LayoutState rowState = state;
        rowState.x = rowStartX;
        if (row->props.hasVerticalAlignment_)
        {
          short childHeight = PreferredChildHeightForRow(child, rowHeight);
          short remain = static_cast<short>(rowHeight - childHeight);
          short offset = 0;
          if (remain > 0)
          {
            if (row->props.verticalAlignment_ == loka::app::VERTICAL_ALIGNMENT_CENTER)
            {
              offset = static_cast<short>(remain / 2);
            }
            else if (row->props.verticalAlignment_ == loka::app::VERTICAL_ALIGNMENT_BOTTOM)
            {
              offset = remain;
            }
          }
          rowState.y = static_cast<short>(state.y + offset);
          rowState.height = childHeight;
        }
        short width = LayoutNode(child, rowState, controller, activeBoundary);
        rowStartX = static_cast<short>(rowStartX + width + state.spacing);
        if (rowState.y > state.y &&
            static_cast<short>(rowState.y - state.y) > maxHeight)
        {
          maxHeight = static_cast<short>(rowState.y - state.y);
        }
      }
      state.y = static_cast<short>(state.y + maxHeight + state.spacing);
      short width = static_cast<short>(rowStartX - state.x);
      if (boundary)
      {
        boundary->setLayoutBounds(startX, startTop, width, static_cast<short>(state.y - startTop));
      }
      return width;
    }
    case loka::app::scene::NODE_KIND_TEXT:
    {
      loka::app::TextNode *text = static_cast<loka::app::TextNode *>(node);
      if (controller && controller->contextMapper())
      {
        controller->contextMapper()->ensureTextContext(text);
      }
      if (text->getContext())
      {
        ToolboxTextContext *ctx = static_cast<ToolboxTextContext *>(text->getContext());
        ctx->setBoundary(activeBoundary);
      }
      short width = node->layout(controller, state);
      if (boundary)
      {
        boundary->setLayoutBounds(startX, startTop, width, static_cast<short>(state.y - startTop));
      }
      return width;
    }
    case loka::app::scene::NODE_KIND_CELL:
    {
      loka::app::CellNode *cell = static_cast<loka::app::CellNode *>(node);
      if (controller && controller->contextMapper())
      {
        controller->contextMapper()->ensureCellContext(cell);
      }
      if (cell->getContext())
      {
        ToolboxCellContext *ctx = static_cast<ToolboxCellContext *>(cell->getContext());
        ctx->setBoundary(activeBoundary);
      }
      short width = node->layout(controller, state);
      if (boundary)
      {
        boundary->setLayoutBounds(startX, startTop, width, static_cast<short>(state.y - startTop));
      }
      return width;
    }
    case loka::app::scene::NODE_KIND_BUTTON:
    {
      loka::app::ButtonNode *button = static_cast<loka::app::ButtonNode *>(node);
      if (controller && controller->contextMapper())
      {
        controller->contextMapper()->ensureButtonContext(button);
      }
      if (button->getContext())
      {
        ToolboxButtonContext *ctx = static_cast<ToolboxButtonContext *>(button->getContext());
        ctx->setBoundary(activeBoundary);
      }
      short width = node->layout(controller, state);
      if (boundary)
      {
        boundary->setLayoutBounds(startX, startTop, width, static_cast<short>(state.y - startTop));
      }
      return width;
    }
    case loka::app::scene::NODE_KIND_EDIT_TEXT:
    {
      loka::app::EditTextNode *edit = static_cast<loka::app::EditTextNode *>(node);
      if (controller && controller->contextMapper())
      {
        controller->contextMapper()->ensureEditTextContext(edit);
      }
      if (edit->getContext())
      {
        ToolboxEditTextContext *ctx = static_cast<ToolboxEditTextContext *>(edit->getContext());
        ctx->setBoundary(activeBoundary);
      }
      short width = node->layout(controller, state);
      if (boundary)
      {
        boundary->setLayoutBounds(startX, startTop, width, static_cast<short>(state.y - startTop));
      }
      return width;
    }
    case loka::app::scene::NODE_KIND_POPUP_MENU:
    {
      loka::app::PopupMenuNode *popup = static_cast<loka::app::PopupMenuNode *>(node);
      if (controller && controller->contextMapper())
      {
        controller->contextMapper()->ensurePopupMenuContext(popup);
      }
      if (popup->getContext())
      {
        ToolboxPopupMenuContext *ctx = static_cast<ToolboxPopupMenuContext *>(popup->getContext());
        ctx->setBoundary(activeBoundary);
      }
      short width = node->layout(controller, state);
      if (boundary)
      {
        boundary->setLayoutBounds(startX, startTop, width, static_cast<short>(state.y - startTop));
      }
      return width;
    }
    case loka::app::scene::NODE_KIND_OPEN_FILE_DIALOG:
    {
      loka::app::OpenFileDialogNode *dialog = static_cast<loka::app::OpenFileDialogNode *>(node);
      if (controller && controller->contextMapper())
      {
        controller->contextMapper()->ensureOpenFileDialogContext(dialog);
      }
      if (boundary)
      {
        boundary->setLayoutBounds(startX, startTop, 0, static_cast<short>(state.y - startTop));
      }
      return 0;
    }
    case loka::app::scene::NODE_KIND_IMAGE_VIEW:
    {
      loka::app::ImageViewNode *image = static_cast<loka::app::ImageViewNode *>(node);
      if (controller && controller->contextMapper())
      {
        controller->contextMapper()->ensureImageViewContext(image);
      }
      if (image->getContext())
      {
        ToolboxImageViewContext *ctx = static_cast<ToolboxImageViewContext *>(image->getContext());
        ctx->setBoundary(activeBoundary);
      }
      short width = node->layout(controller, state);
      if (boundary)
      {
        boundary->setLayoutBounds(startX, startTop, width, static_cast<short>(state.y - startTop));
      }
      return width;
    }
    default:
      break;
    }
    short width = LayoutChildren(node->asNestable(), state, controller, activeBoundary);
    if (boundary)
    {
      boundary->setLayoutBounds(startX, startTop, width, static_cast<short>(state.y - startTop));
    }
    return width;
  }

  void RenderChildren(loka::app::scene::INestable *nestable,
                      ToolboxScenePlatformController *controller)
  {
    if (!nestable)
    {
      return;
    }
    loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
    for (loka::app::scene::Node *child = it.next(); child; child = it.next())
    {
      RenderNode(child, controller);
    }
  }

  void RenderNode(loka::app::scene::Node *node,
                  ToolboxScenePlatformController *controller)
  {
    if (!node)
    {
      return;
    }
    switch (node->kind())
    {
    case loka::app::scene::NODE_KIND_COLUMN:
      RenderChildren(node->asNestable(), controller);
      return;
    case loka::app::scene::NODE_KIND_ROW:
      RenderChildren(node->asNestable(), controller);
      return;
    case loka::app::scene::NODE_KIND_TEXT:
    case loka::app::scene::NODE_KIND_CELL:
    case loka::app::scene::NODE_KIND_BUTTON:
    case loka::app::scene::NODE_KIND_EDIT_TEXT:
    case loka::app::scene::NODE_KIND_POPUP_MENU:
    case loka::app::scene::NODE_KIND_IMAGE_VIEW:
      node->render(controller);
      return;
    case loka::app::scene::NODE_KIND_OPEN_FILE_DIALOG:
      return;
    default:
      break;
    }
    RenderChildren(node->asNestable(), controller);
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
}

ToolboxScenePlatformController::ToolboxScenePlatformController(ToolboxWindow *window)
    : window_(window),
      rootNode_(0),
      focusedText_(0),
      focusedEdit_(0),
      focusedRect_(),
      hasFocusedRect_(false),
      inBatchUpdate_(false),
      pendingFullInvalidate_(false),
      pendingInvalidateFlags_(loka::app::scene::NODE_DIRTY_NONE),
      forceFullRedraw_(false),
      pendingDirtyRects_(),
      retiredControls_(),
      retiredTextEdits_(),
      clipRgn_(NewRgn()),
      hasClip_(false),
      nextControlId_(kAutoControlBaseId)
{
}

ToolboxScenePlatformController::~ToolboxScenePlatformController()
{
  clearTextBindings();
  clearControls();
  flushRetiredNativeHandles();
  if (clipRgn_)
  {
    DisposeRgn(clipRgn_);
    clipRgn_ = 0;
  }
}

ToolboxNodeContextMapper *ToolboxScenePlatformController::contextMapper() const
{
  if (!window_ || !window_->context())
  {
    return 0;
  }
  return window_->context()->contextMapper();
}

short ToolboxScenePlatformController::allocateControlId()
{
  if (nextControlId_ < kAutoControlBaseId)
  {
    nextControlId_ = kAutoControlBaseId;
  }
  short id = nextControlId_;
  ++nextControlId_;
  return id;
}

void ToolboxScenePlatformController::onChange(loka::app::scene::Node *rootNode, loka::app::scene::NodeDirtyFlags flags, bool fullRebuild)
{
  rootNode_ = rootNode;
  if (!window_ || !window_->window())
  {
    return;
  }
  if (inBatchUpdate_)
  {
    pendingInvalidateFlags_ = static_cast<loka::app::scene::NodeDirtyFlags>(pendingInvalidateFlags_ | flags);
    if (flags != loka::app::scene::NODE_DIRTY_NONE || fullRebuild)
    {
      pendingFullInvalidate_ = true;
    }
    return;
  }
  requestInvalidateForChange(flags, fullRebuild);
}

void ToolboxScenePlatformController::synchronize()
{
  // Toolbox doesn't have a retained scene graph; rely on Update events.
}

bool ToolboxScenePlatformController::hasPendingSync() const
{
  return false;
}

void ToolboxScenePlatformController::destroy()
{
  rootNode_ = 0;
  popupHits_.clear();
  clearTextBindings();
  clearControls();
  flushRetiredNativeHandles();
}

void ToolboxScenePlatformController::render()
{
  PROFILE_FUNC();
  if (!window_ || !window_->window() || !rootNode_)
  {
    return;
  }
  {
    short maxExplicit = MaxExplicitControlId(rootNode_);
    if (maxExplicit < kAutoControlBaseId - 1)
    {
      maxExplicit = static_cast<short>(kAutoControlBaseId - 1);
    }
    nextControlId_ = static_cast<short>(maxExplicit + 1);
  }
  buttonHits_.clear();
  cellHits_.clear();
  for (size_t i = 0; i < buttonControls_.size(); ++i)
  {
    buttonControls_[i].usedThisFrame = false;
  }
  editHits_.clear();
  for (size_t i = 0; i < editControls_.size(); ++i)
  {
    editControls_[i].usedThisFrame = false;
  }
  textHits_.clear();
  popupHits_.clear();
  pendingTextStates_.clear();
  pendingDirtyRects_.clear();
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
  PROFILE_SECTION("layout");
  LayoutNode(rootNode_, state, this, 0);
  RenderNode(rootNode_, this);
  for (size_t i = 0; i < buttonControls_.size(); ++i)
  {
    if (!buttonControls_[i].usedThisFrame && buttonControls_[i].control)
    {
      HideControl(buttonControls_[i].control);
    }
  }
  for (size_t i = 0; i < editControls_.size();)
  {
    if (!editControls_[i].usedThisFrame)
    {
      if (&editControls_[i] == focusedEdit_)
      {
        if (focusedEdit_->te)
        {
          TEDeactivate(focusedEdit_->te);
        }
        focusedEdit_ = 0;
      }
      if (editControls_[i].te)
      {
        TEDeactivate(editControls_[i].te);
        queueRetiredTextEdit(editControls_[i].te);
      }
      editControls_[i].te = 0;
      editControls_.erase(editControls_.begin() + i);
      continue;
    }
    ++i;
  }

#if LOKA_PROFILE_FUNC_TICKS
  // Capture profile once on first render and dump to file.
  if (!sProfileCaptured)
  {
    BuildProfileResultString();
    DumpFuncProfileToFile("profile.txt");
    sProfileCaptured = true;
  }
#endif
}

void ToolboxScenePlatformController::renderDirty(const Rect &rect)
{
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
  if (textHits_.empty() && popupHits_.empty() && buttonControls_.empty() && editControls_.empty())
  {
    render();
    return;
  }
  for (size_t i = 0; i < popupHits_.size(); ++i)
  {
    PopupHit &hit = popupHits_[i];
    if (!RectsIntersect(rect, hit.rect))
    {
      continue;
    }
    redrawPopupHit(hit);
  }
  for (size_t i = 0; i < cellHits_.size(); ++i)
  {
    CellHit &hit = cellHits_[i];
    if (!hit.context)
    {
      continue;
    }
    if (!RectsIntersect(rect, hit.rect))
    {
      continue;
    }
    hit.context->draw(this);
  }
  for (size_t i = 0; i < textHits_.size(); ++i)
  {
    TextHit &hit = textHits_[i];
    if (!RectsIntersect(rect, hit.rect))
    {
      continue;
    }
    redrawTextHit(hit);
  }
  for (size_t i = 0; i < editControls_.size(); ++i)
  {
    EditTextControlBinding &binding = editControls_[i];
    if (!binding.te || !binding.usedThisFrame)
    {
      continue;
    }
    if (!RectsIntersect(rect, binding.rect))
    {
      continue;
    }
    TEUpdate(&binding.rect, binding.te);
    FrameRect(&binding.rect);
  }
  drawControlsInRect(rect);
}

bool ToolboxScenePlatformController::handleMouseDown(const Point &point)
{
  if (handleControlClick(point))
  {
    return false;
  }
  if (focusedEdit_ && focusedEdit_->te)
  {
    TEDeactivate(focusedEdit_->te);
    focusedEdit_ = 0;
  }
  for (size_t i = 0; i < editControls_.size(); ++i)
  {
    EditTextControlBinding &binding = editControls_[i];
    if (binding.te && PtInRect(point, &binding.rect))
    {
      focusedEdit_ = &binding;
      TEActivate(binding.te);
      TEClick(point, false, binding.te);
      return true;
    }
  }
  for (size_t i = 0; i < editHits_.size(); ++i)
  {
    EditHit &hit = editHits_[i];
    if (hit.text && PtInRect(point, &hit.rect))
    {
      focusedText_ = hit.text;
      focusedRect_ = hit.rect;
      hasFocusedRect_ = true;
      return true;
    }
  }
  focusedText_ = 0;
  hasFocusedRect_ = false;
  for (size_t i = 0; i < popupHits_.size(); ++i)
  {
    PopupHit &hit = popupHits_[i];
    if (hit.enabled && !hit.enabled->get())
    {
      continue;
    }
    if (!hit.items || hit.items->size() == 0 || !hit.selectedIndex)
    {
      continue;
    }
    if (!PtInRect(point, &hit.rect))
    {
      continue;
    }
    MenuHandle menu = NewMenu(hit.menuId, "\p");
    if (!menu)
    {
      return false;
    }
    for (std::size_t j = 0; j < hit.items->size(); ++j)
    {
      Str255 text;
      std::string utf8;
      if (!loka::platform::CollectUtf8((*hit.items)[j], utf8))
      {
        text[0] = 0;
      }
      else
      {
        std::size_t length = utf8.size();
        if (length > 255)
        {
          length = 255;
        }
        text[0] = static_cast<unsigned char>(length);
        if (length > 0)
        {
          std::memcpy(text + 1, utf8.data(), length);
        }
      }
      AppendMenu(menu, text);
    }
    InsertMenu(menu, -1);
    short currentIndex = 0;
    if (hit.selectedIndex->get() > 0)
    {
      currentIndex = static_cast<short>(hit.selectedIndex->get());
    }
    if (static_cast<std::size_t>(currentIndex) >= hit.items->size())
    {
      currentIndex = static_cast<short>(hit.items->size() - 1);
    }
    Point globalPoint = point;
    LocalToGlobal(&globalPoint);
    long choice = PopUpMenuSelect(menu, globalPoint.v, globalPoint.h, static_cast<short>(currentIndex + 1));
    short item = static_cast<short>(choice & 0xFFFF);
    if (item > 0)
    {
      applyPopupSelectionChange(hit.rect, hit.boundary, hit.selectedIndex, hit.onChange, static_cast<int>(item - 1));
    }
    DeleteMenu(hit.menuId);
    DisposeMenu(menu);
    return false;
  }
  for (size_t i = 0; i < cellHits_.size(); ++i)
  {
    CellHit &hit = cellHits_[i];
    if (!hit.emitter)
    {
      continue;
    }
    if (PtInRect(point, &hit.rect))
    {
      beginBatchUpdate();
      hit.emitter->emit();
      endBatchUpdate();
      return false;
    }
  }
  for (size_t i = 0; i < buttonHits_.size(); ++i)
  {
    ButtonHit &hit = buttonHits_[i];
    if (!hit.emitter)
    {
      continue;
    }
    if (hit.enabled && !hit.enabled->get())
    {
      continue;
    }
    if (PtInRect(point, &hit.rect))
    {
      beginBatchUpdate();
      hit.emitter->emit();
      endBatchUpdate();
      return false;
    }
  }
  return false;
}

bool ToolboxScenePlatformController::handleKeyDown(char key)
{
  if (focusedEdit_ && focusedEdit_->te)
  {
    beginBatchUpdate();
    TEKey(key, focusedEdit_->te);
    updateStateFromEdit(*focusedEdit_);
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

void ToolboxScenePlatformController::recordButtonHit(const Rect &rect,
                                                     loka::core::EmitterState *emitter,
                                                     loka::core::State<bool> *enabled,
                                                     loka::app::scene::BoundaryNode *boundary)
{
  if (!emitter)
  {
    return;
  }
  ButtonHit hit;
  hit.rect = rect;
  hit.emitter = emitter;
  hit.enabled = enabled;
  hit.boundary = boundary;
  buttonHits_.push_back(hit);
}

void ToolboxScenePlatformController::recordCellHit(const Rect &rect,
                                                   loka::core::EmitterState *emitter,
                                                   loka::app::scene::BoundaryNode *boundary,
                                                   ToolboxCellContext *context,
                                                   loka::core::State<loka::core::String> *text)
{
  CellHit hit;
  hit.rect = rect;
  hit.emitter = emitter;
  hit.boundary = boundary;
  hit.context = context;
  hit.text = text;
  cellHits_.push_back(hit);
  bindTextState(text);
}

void ToolboxScenePlatformController::recordEditHit(const Rect &rect,
                                                   loka::core::State<loka::core::String> *text,
                                                   loka::app::scene::BoundaryNode *boundary)
{
  EditHit hit;
  hit.rect = rect;
  hit.text = text;
  hit.boundary = boundary;
  editHits_.push_back(hit);
  bindTextState(text);
  if (text && focusedText_ == text)
  {
    focusedRect_ = rect;
    hasFocusedRect_ = true;
  }
}

void ToolboxScenePlatformController::recordTextHit(const Rect &rect,
                                                   short x,
                                                   short y,
                                                   loka::core::State<loka::core::String> *text,
                                                   loka::app::scene::BoundaryNode *boundary,
                                                   bool needsRelayoutOnChange)
{
  if (!text)
  {
    return;
  }
  TextHit hit;
  hit.rect = rect;
  hit.x = x;
  hit.y = y;
  hit.text = text;
  hit.boundary = boundary;
  hit.lastMeasuredWidth = static_cast<short>(rect.right - rect.left);
  hit.needsRelayoutOnChange = needsRelayoutOnChange;
  textHits_.push_back(hit);
  bindTextState(text);
}

void ToolboxScenePlatformController::recordPopupHit(const Rect &rect,
                                                    short lineHeight,
                                                    const loka::Vector<loka::core::String> *items,
                                                    loka::core::State<int> *selectedIndex,
                                                    loka::core::EmitterState *onChange,
                                                    loka::core::State<bool> *enabled,
                                                    loka::app::scene::BoundaryNode *boundary,
                                                    short menuId)
{
  PopupHit hit;
  hit.rect = rect;
  hit.lineHeight = lineHeight;
  hit.items = items;
  hit.selectedIndex = selectedIndex;
  hit.onChange = onChange;
  hit.enabled = enabled;
  hit.boundary = boundary;
  hit.menuId = menuId;
  popupHits_.push_back(hit);
}

void ToolboxScenePlatformController::applyPopupSelectionChange(const Rect &rect,
                                                               loka::app::scene::BoundaryNode *boundary,
                                                               loka::core::State<int> *selectedIndex,
                                                               loka::core::EmitterState *onChange,
                                                               int newIndex)
{
  if (!selectedIndex)
  {
    return;
  }
  loka::core::MutableState<int> *mutableIndex = static_cast<loka::core::MutableState<int> *>(selectedIndex->asMutableState());
  if (!mutableIndex)
  {
    return;
  }
  beginBatchUpdate();
  mutableIndex->set(newIndex, true);
  if (onChange)
  {
    onChange->emit();
  }
  endBatchUpdate();
  if (window_)
  {
    // Only redraw the popup rect, not the entire boundary
    // Changed texts are handled separately via handleTextChanged
    window_->drawDirty(rect);
  }
  // Note: Changed texts are handled via pendingTextStates_ in endBatchUpdate
  // and subsequent scene invalidation cycle. No need to redraw ALL textHits.
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

void ToolboxScenePlatformController::handleTextChanged(loka::core::State<loka::core::String> *text)
{
  if (!window_)
  {
    return;
  }
  for (size_t i = 0; i < cellHits_.size(); ++i)
  {
    CellHit &hit = cellHits_[i];
    if (hit.text == text)
    {
      if (inBatchUpdate_)
      {
        addPendingDirty(hit.rect);
      }
      else
      {
        window_->drawDirty(hit.rect);
      }
      return;
    }
  }
  for (size_t i = 0; i < textHits_.size(); ++i)
  {
    TextHit &hit = textHits_[i];
    if (hit.text == text)
    {
      if (hit.needsRelayoutOnChange)
      {
        if (inBatchUpdate_)
        {
          pendingFullInvalidate_ = true;
        }
        else
        {
          window_->requestInvalidate();
        }
        return;
      }
      // Just redraw the text rect (no width checking for now)
      if (inBatchUpdate_)
      {
        addPendingDirty(hit.rect);
      }
      else
      {
        window_->drawDirty(hit.rect);
      }
      return;
    }
  }
  for (size_t i = 0; i < editHits_.size(); ++i)
  {
    EditHit &hit = editHits_[i];
    if (hit.text == text)
    {
      // Use text's own rect
      if (inBatchUpdate_)
      {
        addPendingDirty(hit.rect);
      }
      else
      {
        window_->drawDirty(hit.rect);
      }
      return;
    }
  }
  for (size_t i = 0; i < editControls_.size(); ++i)
  {
    if (editControls_[i].text == text)
    {
      if (inBatchUpdate_)
      {
        addPendingDirty(editControls_[i].rect);
        return;
      }
      syncEditTextFromState(editControls_[i]);
      window_->drawDirty(editControls_[i].rect);
      return;
    }
  }
  // State not found in current textHits_/editHits_/editControls_.
  // Add to pending list; will be resolved after next render populates textHits_.
  if (inBatchUpdate_)
  {
    addPendingText(text);
  }
  else
  {
    // State not found, but scene invalidation will handle it
    // through normal recompose cycle. No need for full redraw.
  }
}

void ToolboxScenePlatformController::beginBatchUpdate()
{
  inBatchUpdate_ = true;
  pendingDirtyRects_.clear();
  pendingTextStates_.clear();
  pendingFullInvalidate_ = false;
  pendingInvalidateFlags_ = loka::app::scene::NODE_DIRTY_NONE;
}

void ToolboxScenePlatformController::endBatchUpdate()
{
  inBatchUpdate_ = false;
  if (window_)
  {
    // Draw pending dirty rects without forcing full render
    // The textHits_ from previous render should still be valid for positions
    for (size_t i = 0; i < pendingDirtyRects_.size(); ++i)
    {
      window_->drawDirty(pendingDirtyRects_[i]);
    }
    for (size_t i = 0; i < pendingTextStates_.size(); ++i)
    {
      redrawTextFor(pendingTextStates_[i]);
    }
    requestInvalidateForChange(pendingInvalidateFlags_, pendingFullInvalidate_);
  }
  pendingDirtyRects_.clear();
  pendingTextStates_.clear();
  pendingFullInvalidate_ = false;
  pendingInvalidateFlags_ = loka::app::scene::NODE_DIRTY_NONE;
}

void ToolboxScenePlatformController::addPendingDirty(const Rect &rect)
{
  for (size_t i = 0; i < pendingDirtyRects_.size(); ++i)
  {
    Rect &pending = pendingDirtyRects_[i];
    if (rect.right < pending.left || rect.left > pending.right ||
        rect.bottom < pending.top || rect.top > pending.bottom)
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

void ToolboxScenePlatformController::requestInvalidateForChange(loka::app::scene::NodeDirtyFlags flags, bool fullRebuild)
{
  if (!window_ || !window_->window())
  {
    return;
  }
  if ((flags == loka::app::scene::NODE_DIRTY_NONE) && !fullRebuild)
  {
    return;
  }
  if (fullRebuild || !rootNode_)
  {
    window_->requestInvalidate();
    return;
  }

  Rect fallback = window_->window()->portRect;
  bool queued = false;
  if (flags & loka::app::scene::NODE_DIRTY_CHILD)
  {
    queued = collectLocalBoundaryDirtyRects(rootNode_, fallback);
  }
  if (!queued)
  {
    loka::app::scene::BoundaryNode *boundary = rootNode_->asBoundary();
    if (boundary && boundary->hasLayoutBounds())
    {
      window_->requestInvalidateRect(BoundaryToRect(boundary, fallback));
    }
    else
    {
      window_->requestInvalidate();
    }
  }
}

void ToolboxScenePlatformController::redrawTextHit(const TextHit &hit)
{
  if (!window_ || !window_->window() || !hit.text)
  {
    return;
  }
  GrafPtr oldPort;
  GetPort(&oldPort);
  SetPort(window_->window());
  EraseRect(&hit.rect);
  DrawStringAt(hit.x, hit.y, hit.text->get());
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

void ToolboxScenePlatformController::redrawTextFor(loka::core::State<loka::core::String> *text)
{
  if (!text)
  {
    return;
  }
  for (size_t i = 0; i < textHits_.size(); ++i)
  {
    if (textHits_[i].text == text)
    {
      redrawTextHit(textHits_[i]);
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
      binding->state = 0;
      binding->controller = 0;
    }
  }
  textBindings_.clear();
  boundTextStates_.clear();
  textHits_.clear();
}

void ToolboxScenePlatformController::clearControls()
{
  for (size_t i = 0; i < buttonControls_.size(); ++i)
  {
    if (buttonControls_[i].control)
    {
      HideControl(buttonControls_[i].control);
      queueRetiredControl(buttonControls_[i].control);
      buttonControls_[i].control = 0;
    }
  }
  buttonControls_.clear();
  for (size_t i = 0; i < editControls_.size(); ++i)
  {
    if (editControls_[i].te)
    {
      TEDeactivate(editControls_[i].te);
      queueRetiredTextEdit(editControls_[i].te);
      editControls_[i].te = 0;
    }
  }
  editControls_.clear();
  focusedEdit_ = 0;
}

void ToolboxScenePlatformController::queueRetiredControl(ControlRef control)
{
  if (!control)
  {
    return;
  }
  for (size_t i = 0; i < retiredControls_.size(); ++i)
  {
    if (retiredControls_[i] == control)
    {
      return;
    }
  }
  retiredControls_.push_back(control);
}

void ToolboxScenePlatformController::queueRetiredTextEdit(TEHandle te)
{
  if (!te)
  {
    return;
  }
  for (size_t i = 0; i < retiredTextEdits_.size(); ++i)
  {
    if (retiredTextEdits_[i] == te)
    {
      return;
    }
  }
  retiredTextEdits_.push_back(te);
}

void ToolboxScenePlatformController::flushRetiredNativeHandles()
{
  for (size_t i = 0; i < retiredControls_.size(); ++i)
  {
    if (retiredControls_[i])
    {
      DisposeControl(retiredControls_[i]);
    }
  }
  retiredControls_.clear();
  for (size_t i = 0; i < retiredTextEdits_.size(); ++i)
  {
    if (retiredTextEdits_[i])
    {
      TEDispose(retiredTextEdits_[i]);
    }
  }
  retiredTextEdits_.clear();
}

bool ToolboxScenePlatformController::ensureButtonControl(
    short resourceId,
    const Rect &rect,
    const loka::core::String &label,
    loka::core::EmitterState *emitter,
    loka::core::State<bool> *enabled)
{
  if (!window_ || !window_->window() || resourceId <= 0)
  {
    return false;
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
    Rect rectCopy = rect;
    Str255 title;
    title[0] = 0;
    ControlRef control = NewControl(window_->window(),
                                    &rectCopy,
                                    title,
                                    false,
                                    0,
                                    0,
                                    1,
                                    pushButProc,
                                    0);
    if (!control)
    {
      return false;
    }
    HideControl(control);
    ButtonControlBinding entry;
    entry.resourceId = resourceId;
    entry.control = control;
    entry.emitter = emitter;
    entry.enabled = enabled;
    entry.usedThisFrame = true;
    entry.needsDraw = true;
    entry.rect = rect;
    entry.label = "";
    buttonControls_.push_back(entry);
    binding = &buttonControls_.back();
    created = true;
  }
  binding->emitter = emitter;
  binding->enabled = enabled;
  binding->usedThisFrame = true;
  if (created ||
      binding->rect.left != rect.left || binding->rect.top != rect.top ||
      binding->rect.right != rect.right || binding->rect.bottom != rect.bottom)
  {
    MoveControl(binding->control, rect.left, rect.top);
    SizeControl(binding->control, rect.right - rect.left, rect.bottom - rect.top);
    binding->rect = rect;
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

void ToolboxScenePlatformController::drawFallbackControl(const Rect &rect)
{
  FrameRect(&rect);
  MoveTo(rect.left + 2, rect.top + 2);
  LineTo(rect.right - 2, rect.bottom - 2);
  MoveTo(rect.left + 2, rect.bottom - 2);
  LineTo(rect.right - 2, rect.top + 2);
}

TEHandle ToolboxScenePlatformController::ensureEditTextControl(
    const Rect &rect,
    loka::core::State<loka::core::String> *text)
{
  if (!text)
  {
    return 0;
  }
  EditTextControlBinding *binding = 0;
  for (size_t i = 0; i < editControls_.size(); ++i)
  {
    if (editControls_[i].text == text)
    {
      binding = &editControls_[i];
      break;
    }
  }
  if (!binding)
  {
    TEHandle te = TENew(&rect, &rect);
    if (!te)
    {
    return 0;
  }
    EditTextControlBinding entry;
    entry.text = text;
    entry.te = te;
    entry.rect = rect;
    entry.usedThisFrame = true;
    entry.lastText = "";
    editControls_.push_back(entry);
    binding = &editControls_.back();
    syncEditTextFromState(*binding);
    TEAutoView(true, binding->te);
  }
  binding->usedThisFrame = true;
  if (binding->rect.left != rect.left || binding->rect.top != rect.top ||
      binding->rect.right != rect.right || binding->rect.bottom != rect.bottom)
  {
    binding->rect = rect;
    if (binding->te)
    {
      (**binding->te).destRect = rect;
      (**binding->te).viewRect = rect;
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
    const char *ptr = *textHandle;
    utf8.assign(ptr, static_cast<size_t>(length));
    HUnlock(reinterpret_cast<Handle>(textHandle));
  }
  loka::core::StateTrackerGuard _(window_ ? window_->getTracker() : 0);
  mutableText->set(loka::core::String(utf8));
  binding.lastText = utf8;
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
    if (rect.right < binding.rect.left || rect.left > binding.rect.right ||
        rect.bottom < binding.rect.top || rect.top > binding.rect.bottom)
    {
      continue;
    }
    Draw1Control(binding.control);
    binding.needsDraw = false;
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
  for (size_t i = 0; i < editHits_.size(); ++i)
  {
    const EditHit &hit = editHits_[i];
    if (hit.text && PtInRect(point, &hit.rect))
    {
      return true;
    }
  }
  return false;
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

bool ToolboxScenePlatformController::handleControlClick(const Point &point)
{
  if (!window_ || !window_->window())
  {
    return false;
  }
  ControlRef control = 0;
  ControlPartCode part = FindControl(point, window_->window(), &control);
  if (part == 0 || !control)
  {
    return false;
  }
  for (size_t i = 0; i < buttonControls_.size(); ++i)
  {
    ButtonControlBinding &binding = buttonControls_[i];
    if (binding.control == control && binding.emitter)
    {
      if (binding.enabled && !binding.enabled->get())
      {
        return true;
      }
      beginBatchUpdate();
      ControlPartCode tracked = TrackControl(control, point, 0);
      if (tracked != 0)
      {
        binding.emitter->emit();
      }
      endBatchUpdate();
      return true;
    }
  }
  return false;
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
