#ifndef LOKA_APP_LAYOUT_CONTAINER_LAYOUT_HPP
#define LOKA_APP_LAYOUT_CONTAINER_LAYOUT_HPP

#include "app/Box.hpp"
#include "app/RowColumn.hpp"
#include "app/ZStack.hpp"
#include "app/layout/LayoutHeuristics.hpp"
#include "loka/dsl/CompositionList.hpp"

namespace loka
{
  namespace app
  {
    namespace layout
    {
      struct RowLayoutMetrics
      {
        int gap;
        int fallbackHeight;
        int buttonHeight;
        int editTextHeight;
        int popupMenuHeight;
        int textHeight;
        int imageFallbackHeight;

        RowLayoutMetrics()
            : gap(0),
              fallbackHeight(0),
              buttonHeight(0),
              editTextHeight(0),
              popupMenuHeight(0),
              textHeight(0),
              imageFallbackHeight(0)
        {
        }
      };

      template <typename LayoutStateT>
      int computeBoxLayoutResultY(loka::app::BoxNode *box,
                                  const LayoutStateT &state,
                                  void *context,
                                  int (*layoutChild)(void *, loka::app::scene::Node *, const LayoutStateT &))
      {
        if (!box)
        {
          return state.y;
        }

        const int padding = box->props.padding;
        LayoutStateT childState = state;
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
            childState.y = layoutChild(context, child, childState);
          }
          resultY = childState.y + padding;
        }
        else
        {
          resultY = state.y + padding * 2;
        }
        return resultY;
      }

      template <typename LayoutStateT>
      int computeZStackLayoutResultY(loka::app::ZStackNode *stack,
                                     const LayoutStateT &state,
                                     void *context,
                                     int (*layoutChild)(void *, loka::app::scene::Node *, const LayoutStateT &))
      {
        if (!stack)
        {
          return state.y;
        }

        int maxY = state.y;
        if (loka::app::scene::INestable *nestable = stack->asNestable())
        {
          loka::dsl::CompositionCursor<loka::app::scene::Node> it(nestable->childrenHead(), nestable->childrenCount());
          for (loka::app::scene::Node *child = it.next(); child; child = it.next())
          {
            LayoutStateT childState = state;
            const int childY = layoutChild(context, child, childState);
            if (childY > maxY)
            {
              maxY = childY;
            }
          }
        }
        return maxY;
      }

      template <typename LayoutStateT>
      int computeColumnLayoutResultY(loka::app::ColumnNode *column,
                                     const LayoutStateT &state,
                                     void *context,
                                     int (*layoutChild)(void *, loka::app::scene::Node *, const LayoutStateT &))
      {
        if (!column || column->childrenHead() == 0 || column->childrenCount() == 0)
        {
          return state.y;
        }

        int currentY = state.y;
        loka::dsl::CompositionCursor<loka::app::scene::Node> it(column->childrenHead(), column->childrenCount());
        for (loka::app::scene::Node *child = it.next(); child; child = it.next())
        {
          LayoutStateT childState = state;
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
          currentY = layoutChild(context, child, childState);
        }
        return currentY;
      }

      template <typename LayoutStateT>
      int computeRowLayoutResultY(loka::app::RowNode *row,
                                  const LayoutStateT &state,
                                  const RowLayoutMetrics &metrics,
                                  void *context,
                                  int (*layoutChild)(void *, loka::app::scene::Node *, const LayoutStateT &))
      {
        if (!row)
        {
          return state.y;
        }

        const size_t childCount = row->childrenCount();
        if (row->childrenHead() == 0 || childCount == 0)
        {
          return state.y;
        }

        const int childCountInt = static_cast<int>(childCount);
        const int spacingTotal = metrics.gap * (childCountInt > 0 ? childCountInt - 1 : 0);
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
                                                                        metrics.fallbackHeight,
                                                                        metrics.buttonHeight,
                                                                        metrics.editTextHeight,
                                                                        metrics.popupMenuHeight,
                                                                        metrics.textHeight,
                                                                        metrics.imageFallbackHeight);
            if (h > rowHeight)
            {
              rowHeight = h;
            }
          }
          if (rowHeight <= 0)
          {
            rowHeight = metrics.textHeight;
          }
        }

        int currentX = state.x;
        int maxY = state.y;
        loka::dsl::CompositionCursor<loka::app::scene::Node> it(row->childrenHead(), childCount);
        for (loka::app::scene::Node *child = it.next(); child; child = it.next())
        {
          LayoutStateT childState = state;
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
                                                                                   metrics.buttonHeight,
                                                                                   metrics.editTextHeight,
                                                                                   metrics.popupMenuHeight,
                                                                                   metrics.textHeight,
                                                                                   metrics.imageFallbackHeight);
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
          const int childY = layoutChild(context, child, childState);
          if (childY > maxY)
          {
            maxY = childY;
          }
          currentX += childWidth + metrics.gap;
        }
        return maxY;
      }
    }
  }
}

#endif // LOKA_APP_LAYOUT_CONTAINER_LAYOUT_HPP
