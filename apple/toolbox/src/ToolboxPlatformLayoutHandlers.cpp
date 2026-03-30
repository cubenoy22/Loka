#include "ToolboxPlatformLayoutHandlers.hpp"

#include "app/Box.hpp"
#include "app/Grid.hpp"
#include "app/ImageView.hpp"
#include "app/RowColumn.hpp"
#include "app/ZStack.hpp"
#include "app/layout/LayoutHeuristics.hpp"
#include "loka/dsl/CompositionList.hpp"

namespace
{
  static const short kToolboxLayoutImageFallbackHeight = 80;

  inline short ClampToAvailable(short value, short available)
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

  inline short PreferredChildWidthForColumn(loka::app::scene::Node *child, short availableWidth)
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

  inline short PreferredChildHeightForRow(loka::app::scene::Node *child, short fallbackHeight)
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
      return kToolboxLayoutImageFallbackHeight;
    }
    return fallbackHeight;
  }

  inline int DispatchTraversalLayoutChild(void *context,
                                          loka::app::scene::Node *child,
                                          const loka::app::scene::LayoutState &state)
  {
    loka::app::scene::IPlatformLayoutTraversal *traversal =
        static_cast<loka::app::scene::IPlatformLayoutTraversal *>(context);
    if (!traversal)
    {
      return 0;
    }
    return traversal->layoutChild(child, state);
  }

  class ToolboxBoxLayoutHandler : public loka::app::scene::IPlatformLayoutHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::BoxNode>();
    }

    virtual int layoutNode(loka::app::scene::Node *node,
                           const loka::app::scene::LayoutState &state,
                           loka::app::scene::IPlatformLayoutTraversal *traversal)
    {
      loka::app::BoxNode *box = node ? node->asBoxNode() : 0;
      if (!box || !traversal)
      {
        return 0;
      }

      const short padding = static_cast<short>(box->props.padding);
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

      short childWidth = 0;
      short currentY = childState.y;
      if (loka::app::scene::INestable *nestable = box->asNestable())
      {
        loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
        for (loka::app::scene::Node *child = it.next(); child; child = it.next())
        {
          childState.y = currentY;
          const int width = DispatchTraversalLayoutChild(traversal, child, childState);
          if (width > childWidth)
          {
            childWidth = static_cast<short>(width);
          }
          currentY = traversal->layoutResultY();
        }
      }
      traversal->setLayoutResultY(currentY);

      short width = static_cast<short>(childWidth + padding * 2);
      if (childWidth == 0 && box->childrenCount() == 0)
      {
        width = static_cast<short>(padding * 2);
      }
      return width;
    }
  };

  class ToolboxZStackLayoutHandler : public loka::app::scene::IPlatformLayoutHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::ZStackNode>();
    }

    virtual int layoutNode(loka::app::scene::Node *node,
                           const loka::app::scene::LayoutState &state,
                           loka::app::scene::IPlatformLayoutTraversal *traversal)
    {
      loka::app::ZStackNode *stack = node ? node->asZStackNode() : 0;
      if (!stack || !traversal)
      {
        return 0;
      }

      short maxWidth = 0;
      short maxY = state.y;
      if (loka::app::scene::INestable *nestable = stack->asNestable())
      {
        loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
        for (loka::app::scene::Node *child = it.next(); child; child = it.next())
        {
          loka::app::scene::LayoutState childState = state;
          const int width = DispatchTraversalLayoutChild(traversal, child, childState);
          if (width > maxWidth)
          {
            maxWidth = static_cast<short>(width);
          }
          const short childResultY = traversal->layoutResultY();
          if (childResultY > maxY)
          {
            maxY = childResultY;
          }
        }
      }
      traversal->setLayoutResultY(maxY);
      return maxWidth;
    }
  };

  class ToolboxColumnLayoutHandler : public loka::app::scene::IPlatformLayoutHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::ColumnNode>();
    }

    virtual int layoutNode(loka::app::scene::Node *node,
                           const loka::app::scene::LayoutState &state,
                           loka::app::scene::IPlatformLayoutTraversal *traversal)
    {
      loka::app::ColumnNode *column = node ? node->asColumnNode() : 0;
      if (!column || !traversal)
      {
        return 0;
      }

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
        const int childWidthUsed = DispatchTraversalLayoutChild(traversal, child, childState);
        if (childWidthUsed > width)
        {
          width = static_cast<short>(childWidthUsed);
        }
        currentY = traversal->layoutResultY();
      }
      return width;
    }
  };

  class ToolboxRowLayoutHandler : public loka::app::scene::IPlatformLayoutHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::RowNode>();
    }

    virtual int layoutNode(loka::app::scene::Node *node,
                           const loka::app::scene::LayoutState &state,
                           loka::app::scene::IPlatformLayoutTraversal *traversal)
    {
      loka::app::RowNode *row = node ? node->asRowNode() : 0;
      if (!row || !traversal)
      {
        return 0;
      }

      short rowStartX = state.x;
      short maxHeight = 0;
      short rowHeight = state.lineHeight > 0 ? state.lineHeight : 12;
      const size_t childCount = row->childrenCount();
      if (row->props.hasVerticalAlignment_)
      {
        rowHeight = 0;
        loka::dsl::CompositionCursor<loka::app::scene::Node> measure(row->childrenHead(), row->childrenCount());
        for (loka::app::scene::Node *child = measure.next(); child; child = measure.next())
        {
          short height = PreferredChildHeightForRow(child, state.lineHeight > 0 ? state.lineHeight : 12);
          if (height > rowHeight)
          {
            rowHeight = height;
          }
        }
        if (rowHeight <= 0)
        {
          rowHeight = 12;
        }
      }

      loka::dsl::CompositionCursor<loka::app::scene::Node> it(row->childrenHead(), row->childrenCount());
      size_t childIndex = 0;
      for (loka::app::scene::Node *child = it.next(); child; child = it.next(), ++childIndex)
      {
        loka::app::scene::LayoutState rowState = state;
        rowState.x = rowStartX;
        if (state.width > 0)
        {
          const short usedWidth = static_cast<short>(rowStartX - state.x);
          short remainingWidth = static_cast<short>(state.width - usedWidth);
          if (remainingWidth < 0)
          {
            remainingWidth = 0;
          }
          const size_t remainingChildren = (childCount > childIndex) ? (childCount - childIndex) : 1;
          if (remainingChildren > 0)
          {
            rowState.width = static_cast<short>(remainingWidth / static_cast<short>(remainingChildren));
          }
        }
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
        const int width = DispatchTraversalLayoutChild(traversal, child, rowState);
        rowStartX = static_cast<short>(rowStartX + width + state.spacing);
        const short childResultY = traversal->layoutResultY();
        if (childResultY > state.y &&
            static_cast<short>(childResultY - state.y) > maxHeight)
        {
          maxHeight = static_cast<short>(childResultY - state.y);
        }
      }
      traversal->setLayoutResultY(static_cast<short>(state.y + maxHeight + state.spacing));
      return static_cast<short>(rowStartX - state.x);
    }
  };

  class ToolboxGridLayoutHandler : public loka::app::scene::IPlatformLayoutHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::GridNode>();
    }

    virtual int layoutNode(loka::app::scene::Node *node,
                           const loka::app::scene::LayoutState &state,
                           loka::app::scene::IPlatformLayoutTraversal *traversal)
    {
      loka::app::GridNode *grid = node ? node->asGridNode() : 0;
      if (!grid || !traversal)
      {
        return 0;
      }

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
          const int width = DispatchTraversalLayoutChild(traversal, child, cellState);
          if (width > maxWidth)
          {
            maxWidth = static_cast<short>(width);
          }
          const short childResultY = traversal->layoutResultY();
          if (childResultY > maxY)
          {
            maxY = childResultY;
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
      traversal->setLayoutResultY(maxY);
      return maxWidth;
    }
  };
}

void RegisterToolboxPlatformLayoutHandlers(loka::app::scene::PlatformLayoutHandlerRegistry &registry)
{
  registry.registerHandler(new ToolboxBoxLayoutHandler());
  registry.registerHandler(new ToolboxZStackLayoutHandler());
  registry.registerHandler(new ToolboxColumnLayoutHandler());
  registry.registerHandler(new ToolboxRowLayoutHandler());
  registry.registerHandler(new ToolboxGridLayoutHandler());
}
