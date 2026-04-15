#ifndef LOKA_APP_LAYOUT_COLUMN_LAYOUT_HPP
#define LOKA_APP_LAYOUT_COLUMN_LAYOUT_HPP

#include "app/nodes/nestable/RowColumn.hpp"
#include "app/layout/LayoutHeuristics.hpp"
#include "loka/dsl/CompositionList.hpp"

namespace loka
{
  namespace app
  {
    namespace layout
    {
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

#endif // LOKA_APP_LAYOUT_COLUMN_LAYOUT_HPP
