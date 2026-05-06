#ifndef LOKA_APP_LAYOUT_ROW_LAYOUT_HPP
#define LOKA_APP_LAYOUT_ROW_LAYOUT_HPP

#include "app/nodes/nestable/RowColumn.hpp"
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
    } // namespace layout
  } // namespace app
} // namespace loka

#endif // LOKA_APP_LAYOUT_ROW_LAYOUT_HPP
