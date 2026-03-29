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
    }
  }
}

#endif // LOKA_APP_LAYOUT_CONTAINER_LAYOUT_HPP
