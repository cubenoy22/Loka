#include "ToolboxNodeDispatch.hpp"
#include "ToolboxPlatformLayoutHandlers.hpp"
#include "ToolboxScenePlatformController.hpp"
#include "app/RectSurface.hpp"
#include "app/layout/LayoutHeuristics.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/nestable/Grid.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/ScrollView.hpp"
#include "app/nodes/nestable/ZStack.hpp"
#include "context/ToolboxRectSurfaceContext.hpp"

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
      if (controller && controller->refuseNarrowingInScrollScope(state.y))
      {
        break;
      }
      if (width > maxWidth)
      {
        maxWidth = width;
      }
    }
    return maxWidth;
  }

namespace
{
  class ToolboxLayoutTraversal : public loka::app::scene::IPlatformLayoutTraversal
  {
  public:
    ToolboxLayoutTraversal(ToolboxScenePlatformController *controller, loka::app::scene::BoundaryNode *currentBoundary)
        : controller_(controller),
          currentBoundary_(currentBoundary),
          layoutResultY_(0)
    {
    }

    virtual int layoutChild(loka::app::scene::Node *child, const loka::app::scene::LayoutState &state)
    {
      loka::app::scene::LayoutState childState = state;
      const short width = LayoutNode(child, childState, controller_, currentBoundary_);
      if (controller_ && controller_->refuseNarrowingInScrollScope(childState.y))
      {
        return 0;
      }
      layoutResultY_ = childState.y;
      return width;
    }

    virtual void setLayoutResultY(short y)
    {
      layoutResultY_ = y;
    }

    virtual short layoutResultY() const
    {
      return layoutResultY_;
    }

  private:
    ToolboxScenePlatformController *controller_;
    loka::app::scene::BoundaryNode *currentBoundary_;
    short layoutResultY_;
  };

  class ActiveLayoutBoundaryScope
  {
  public:
    ActiveLayoutBoundaryScope(ToolboxScenePlatformController *controller, loka::app::scene::BoundaryNode *boundary)
        : controller_(controller),
          previous_(controller ? controller->activeLayoutBoundary() : 0)
    {
      if (controller_)
      {
        controller_->setActiveLayoutBoundary(boundary);
      }
    }

    ~ActiveLayoutBoundaryScope()
    {
      if (controller_)
      {
        controller_->setActiveLayoutBoundary(previous_);
      }
    }

  private:
    ToolboxScenePlatformController *controller_;
    loka::app::scene::BoundaryNode *previous_;
  };
} // namespace

  short LayoutNode(loka::app::scene::Node *node,
                   loka::app::scene::LayoutState &state,
                   ToolboxScenePlatformController *controller,
                   loka::app::scene::BoundaryNode *currentBoundary)
  {
    if (!node)
    {
      return 0;
    }
    if (controller && controller->refuseNarrowingInScrollScope(state.y))
    {
      return 0;
    }
    loka::app::scene::BoundaryNode *boundary = node->asBoundary();
    loka::app::scene::BoundaryNode *activeBoundary = boundary ? boundary : currentBoundary;
    const short startX = state.x;
    const short startY = state.y;
    const short startTop = static_cast<short>(startY - state.lineHeight + 2);
    if (loka::app::scene::IProjectedLayoutNode *projected = node->asProjectedLayoutNode())
    {
      ActiveLayoutBoundaryScope boundaryScope(controller, activeBoundary);
      short width = projected->layoutProjected(controller, state);
      if (controller && !controller->restoreProjectedLayoutState(state))
      {
        return 0;
      }
      if (boundary)
      {
        boundary->setLayoutBounds(startX, startTop, width, static_cast<short>(state.y - startTop));
      }
      return width;
    }
    switch (node->kind())
    {
    case loka::app::scene::NODE_KIND_STACK:
    {
      loka::app::StackNode *stack = static_cast<loka::app::StackNode *>(node);
      short width = 0;
      bool usedHandler = false;
      if (controller && controller->layoutHandlerRegistry())
      {
        ToolboxLayoutTraversal traversal(controller, activeBoundary);
        usedHandler = ApplyToolboxPlatformLayoutHandler(
            *controller->layoutHandlerRegistry(), *stack, state, traversal, width);
      }
      if (!usedHandler && stack->props.axis_ == loka::app::STACK_AXIS_COLUMN)
      {
        loka::app::StackNode *column = stack;
        short currentY = state.y;
        loka::dsl::CompositionCursor<loka::app::scene::Node> it(column->childrenHead(), column->childrenCount());
        for (loka::app::scene::Node *child = it.next(); child; child = it.next())
        {
          loka::app::scene::LayoutState childState = state;
          childState.y = currentY;
          if (state.height > 0)
          {
            childState.height =
                static_cast<short>(loka::app::layout::remainingChildHeightForColumn(state.height, state.y, currentY));
          }
          short childWidth = state.width;
          short childOffset = 0;
          if (column->props.hasHorizontalAlignment_)
          {
            childWidth = static_cast<short>(
                loka::app::layout::preferredChildWidthForColumn(child, state.width));
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
      }
      else if (!usedHandler)
      {
        ToolboxLayoutTraversal traversal(controller, activeBoundary);
        width = static_cast<short>(ComputeToolboxRowLayout(stack, state, &traversal));
        state.y = traversal.layoutResultY();
      }
      if (boundary)
      {
        boundary->setLayoutBounds(startX, startTop, width, static_cast<short>(state.y - startTop));
      }
      return width;
    }
    case loka::app::scene::NODE_KIND_BOX:
    {
      loka::app::BoxNode *box = static_cast<loka::app::BoxNode *>(node);
      short width = 0;
      bool usedHandler = false;
      if (controller && controller->layoutHandlerRegistry())
      {
        ToolboxLayoutTraversal traversal(controller, activeBoundary);
        usedHandler = ApplyToolboxPlatformLayoutHandler(
            *controller->layoutHandlerRegistry(), *box, state, traversal, width);
      }
      if (!usedHandler)
      {
        short padding = static_cast<short>(box->props.padding);
        const bool hasFixedSize = box->props.hasFixedSize();
        loka::app::scene::LayoutState childState = state;
        childState.x = static_cast<short>(state.x + padding);
        childState.y = static_cast<short>(state.y + padding);
        if (hasFixedSize)
        {
          childState.width = box->props.width;
          childState.height = box->props.height;
        }
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
        width = hasFixedSize ? box->props.width : static_cast<short>(childWidth + padding * 2);
        state.y = hasFixedSize ? static_cast<short>(state.y + box->props.height)
                               : static_cast<short>(childState.y + padding);
      }
      if (boundary)
      {
        boundary->setLayoutBounds(startX, startTop, width, static_cast<short>(state.y - startTop));
      }
      return width;
    }
    case loka::app::scene::NODE_KIND_ZSTACK:
    {
      loka::app::ZStackNode *stack = static_cast<loka::app::ZStackNode *>(node);
      short maxWidth = 0;
      short maxY = state.y;
      bool usedHandler = false;
      if (controller && controller->layoutHandlerRegistry())
      {
        ToolboxLayoutTraversal traversal(controller, activeBoundary);
        usedHandler = ApplyToolboxPlatformLayoutHandler(
            *controller->layoutHandlerRegistry(), *stack, state, traversal, maxWidth);
        if (usedHandler)
        {
          maxY = state.y;
        }
      }
      if (!usedHandler)
      {
        loka::app::scene::LayoutState childState = state;
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
      short maxWidth = 0;
      bool usedHandler = false;
      if (controller && controller->layoutHandlerRegistry())
      {
        ToolboxLayoutTraversal traversal(controller, activeBoundary);
        usedHandler = ApplyToolboxPlatformLayoutHandler(
            *controller->layoutHandlerRegistry(), *grid, state, traversal, maxWidth);
      }
      if (!usedHandler)
      {
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
        maxWidth = static_cast<short>(cellWidth * cols + gap * (cols > 0 ? cols - 1 : 0));
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
      }
      if (boundary)
      {
        boundary->setLayoutBounds(startX, startTop, maxWidth, static_cast<short>(state.y - startTop));
      }
      return maxWidth;
    }
    case loka::app::scene::NODE_KIND_RECT_SURFACE:
    {
      loka::app::RectSurfaceNode *surface = static_cast<loka::app::RectSurfaceNode *>(node);
      if (controller)
      {
        EnsureToolboxRectSurfaceContext(surface, controller);
      }
      if (surface->getContext())
      {
        ToolboxRectSurfaceContext *ctx = static_cast<ToolboxRectSurfaceContext *>(surface->getContext());
        ctx->setBoundary(activeBoundary);
      }
      loka::app::scene::LayoutState projectedState = state;
      const short seatX = state.x;
      const short seatY = static_cast<short>(state.y);
      const short resolvedWidth =
          surface->props.width_ > 0 ? surface->props.width_ : state.width;
      const short resolvedHeight =
          surface->props.height_ > 0 ? surface->props.height_ : state.height;
      projectedState.width = resolvedWidth;
      projectedState.height = resolvedHeight;
      if (controller)
      {
        // RectSurface is the one hand-routed projected leaf in this switch;
        // give it the same translation and restore discipline as handler-
        // backed leaves.
        if (!controller->projectLayoutState(projectedState))
        {
          return 0;
        }
      }
      short width = node->layout(controller, projectedState);
      if (controller && !controller->restoreProjectedLayoutState(projectedState))
      {
        return 0;
      }
      if (controller)
      {
        // The seat is a fact only once the surface was actually placed: a
        // refused projection or restore records nothing.
        controller->recordRectSurfaceExtent(
            surface, loka::core::Frame(seatX, seatY, resolvedWidth, resolvedHeight));
      }
      state.y = projectedState.y;
      if (boundary)
      {
        boundary->setLayoutBounds(startX, startTop, width, static_cast<short>(state.y - startTop));
      }
      return width;
    }
    case loka::app::scene::NODE_KIND_SCROLL_VIEW:
    {
      short width = controller
                        ? controller->layoutScrollView(
                              static_cast<loka::app::ScrollViewNode *>(node),
                              state,
                              activeBoundary)
                        : 0;
      if (boundary)
      {
        boundary->setLayoutBounds(startX, startTop, width,
                                  static_cast<short>(state.y - startTop));
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
  void RenderChildren(loka::app::scene::INestable *nestable, ToolboxScenePlatformController *controller)
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

  void RenderNode(loka::app::scene::Node *node, ToolboxScenePlatformController *controller)
  {
    if (!node)
    {
      return;
    }
    if (node->asProjectedLayoutNode())
    {
      node->render(controller);
      return;
    }
    switch (node->kind())
    {
    case loka::app::scene::NODE_KIND_STACK:
      RenderChildren(node->asNestable(), controller);
      return;
    case loka::app::scene::NODE_KIND_RECT_SURFACE:
      node->render(controller);
      return;
    case loka::app::scene::NODE_KIND_SCROLL_VIEW:
      if (controller)
      {
        controller->renderScrollView(
            static_cast<loka::app::ScrollViewNode *>(node));
      }
      return;
    default:
      break;
    }
    RenderChildren(node->asNestable(), controller);
  }
