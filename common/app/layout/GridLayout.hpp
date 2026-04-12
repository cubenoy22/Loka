#ifndef LOKA_APP_LAYOUT_GRID_LAYOUT_HPP
#define LOKA_APP_LAYOUT_GRID_LAYOUT_HPP

#include "app/Grid.hpp"
#include "loka/dsl/CompositionList.hpp"

namespace loka
{
  namespace app
  {
    namespace layout
    {
      struct GridLayoutMetrics
      {
        int gapX;
        int gapY;

        GridLayoutMetrics() : gapX(0), gapY(0) {}
      };

      template <typename LayoutStateT>
      int computeGridLayoutResultY(loka::app::GridNode *grid,
                                   const LayoutStateT &state,
                                   const GridLayoutMetrics &metrics,
                                   void *context,
                                   int (*layoutChild)(void *, loka::app::scene::Node *, const LayoutStateT &))
      {
        if (!grid)
        {
          return state.y;
        }

        const int rows = grid->props.rows > 0 ? grid->props.rows : 1;
        const int cols = grid->props.cols > 0 ? grid->props.cols : 1;
        const int availableWidth = state.width - metrics.gapX * (cols - 1);
        const int availableHeight = state.height - metrics.gapY * (rows - 1);
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
            const int rowIndex = static_cast<int>(index / cols);
            const int colIndex = static_cast<int>(index % cols);
            LayoutStateT childState = state;
            childState.x = state.x + colIndex * (cellWidth + metrics.gapX);
            childState.y = state.y + rowIndex * (cellHeight + metrics.gapY);
            childState.width = cellWidth;
            childState.height = cellHeight;
            const int childY = layoutChild(context, child, childState);
            if (childY > maxY)
            {
              maxY = childY;
            }
          }
        }
        return maxY;
      }
    }
  }
}

#endif // LOKA_APP_LAYOUT_GRID_LAYOUT_HPP
