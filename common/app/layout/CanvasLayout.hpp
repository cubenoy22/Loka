#ifndef LOKA_APP_LAYOUT_CANVAS_LAYOUT_HPP
#define LOKA_APP_LAYOUT_CANVAS_LAYOUT_HPP

#include <climits>
#include "app/nodes/nestable/Canvas.hpp"
#include "app/scene/projection/PlatformLayoutHandler.hpp"
#include "dsl/composition/CompositionList.hpp"

namespace loka
{
  namespace app
  {
    namespace layout
    {

      /** One geometry implementation for all rails. Return value is the content
          bottom (parent Y + full content height), never the visible subset's height.
          The far-edge row is included, including when it just touches the viewport.
          No translation scope is needed: child coordinates already include the
          viewport subtraction and the parent's placement origin. */
      class CanvasPlatformLayoutHandler : public scene::IPlatformLayoutHandler
      {
      public:
        virtual const void *nodeTypeKey() const
        {
          return scene::NodeTypeToken<CanvasNode>();
        }

        /** Full content extents, checked before multiplication. Empty content is 0x0. */
        static CanvasLayoutStatus contentExtent(const CanvasNode &node, loka::core::Frame &out)
        {
          const CanvasProps &p = node.props;
          if (p.cellWidth <= 0 || p.cellHeight <= 0 || !p.wrap || !p.viewport
              || (p.axis != STACK_AXIS_COLUMN && p.axis != STACK_AXIS_ROW))
            return CANVAS_LAYOUT_INVALID_INPUT;
          const size_t count = node.childrenCount();
          const size_t rows = rowCount(count, p.wrap);
          const size_t columns = count < p.wrap ? count : p.wrap;
          const size_t xCells = p.axis == STACK_AXIS_COLUMN ? columns : rows;
          const size_t yCells = p.axis == STACK_AXIS_COLUMN ? rows : columns;
          if (xCells > static_cast<size_t>(INT_MAX / p.cellWidth)
              || yCells > static_cast<size_t>(INT_MAX / p.cellHeight))
            return CANVAS_LAYOUT_INT_RANGE_REFUSED;
          out =
              loka::core::Frame(0, 0, static_cast<int>(xCells) * p.cellWidth, static_cast<int>(yCells) * p.cellHeight);
          return CANVAS_LAYOUT_READY;
        }

        virtual int
        layoutNode(scene::Node *node, const scene::LayoutState &state, scene::IPlatformLayoutTraversal *traversal)
        {
          CanvasNode *canvas = node ? node->asCanvasNode() : 0;
          if (!canvas || !traversal)
            return state.y;
          loka::core::Frame extent;
          CanvasLayoutStatus status = contentExtent(*canvas, extent);
          int resultY = state.y;
          if (status == CANVAS_LAYOUT_READY && !add(state.y, extent.height, resultY))
            status = CANVAS_LAYOUT_INT_RANGE_REFUSED;
          if (status != CANVAS_LAYOUT_READY)
            return refuse(*canvas, status, state.y);
          const CanvasProps &p = canvas->props;
          const loka::core::Frame viewport = p.viewport->get();
          if (viewport.width < 0 || viewport.height < 0)
            return refuse(*canvas, CANVAS_LAYOUT_INVALID_INPUT, state.y);
          canvas->recordLayoutStatus(CANVAS_LAYOUT_READY);
          if (!viewport.width || !viewport.height || !canvas->childrenCount())
            return resultY;

          const bool vertical = p.axis == STACK_AXIS_COLUMN;
          const int cellMain = vertical ? p.cellHeight : p.cellWidth;
          const int origin = vertical ? viewport.y : viewport.x;
          const int size = vertical ? viewport.height : viewport.width;
          int end = 0;
          if (!add(origin, size, end))
            return refuse(*canvas, CANVAS_LAYOUT_INT_RANGE_REFUSED, state.y);
          if (end < 0)
            return resultY;
          const size_t rows = rowCount(canvas->childrenCount(), p.wrap);
          const size_t firstRow = origin > 0 ? static_cast<size_t>(origin / cellMain) : 0;
          if (firstRow >= rows)
            return resultY;
          size_t lastRow = static_cast<size_t>(end / cellMain);
          if (lastRow >= rows)
            lastRow = rows - 1;
          const size_t first = firstRow * p.wrap;
          const size_t last = lastRow == rows - 1 ? canvas->childrenCount() : (lastRow + 1) * p.wrap;

          // Composition is linked: O(first) to seek once, then O(candidate children).
          loka::dsl::CompositionCursor<scene::Node> cursor(canvas->childrenHead(), canvas->childrenCount());
          for (size_t i = 0; i < first; ++i)
            cursor.next();
          for (size_t i = first; i < last; ++i)
          {
            scene::Node *child = cursor.next();
            if (!child)
              break;
            const int column = static_cast<int>(vertical ? i % p.wrap : i / p.wrap);
            const int row = static_cast<int>(vertical ? i / p.wrap : i % p.wrap);
            int x = 0;
            int y = 0;
            if (!subtract(column * p.cellWidth, viewport.x, x) || !subtract(row * p.cellHeight, viewport.y, y))
              return refuse(*canvas, CANVAS_LAYOUT_INT_RANGE_REFUSED, state.y);
            // Cross-axis rejection uses differences, avoiding overflowing far edges.
            if (x <= -p.cellWidth || x > viewport.width || y <= -p.cellHeight || y > viewport.height)
              continue;
            if (!add(x, state.x, x) || !add(y, state.y, y))
              return refuse(*canvas, CANVAS_LAYOUT_INT_RANGE_REFUSED, state.y);
            if (x < SHRT_MIN || x > SHRT_MAX || y < SHRT_MIN || y > SHRT_MAX)
              return refuse(*canvas, CANVAS_LAYOUT_SHORT_RANGE_REFUSED, state.y);
            scene::LayoutState childState = state;
            childState.x = static_cast<short>(x);
            childState.y = static_cast<short>(y);
            childState.width = p.cellWidth;
            childState.height = p.cellHeight;
            traversal->layoutChild(child, childState);
          }
          return resultY;
        }

      private:
        static size_t rowCount(size_t count, unsigned short wrap)
        {
          return count / wrap + (count % wrap != 0);
        }
        static int refuse(CanvasNode &node, CanvasLayoutStatus status, int result)
        {
          node.recordLayoutStatus(status);
          return result;
        }
        static bool add(int a, int b, int &out)
        {
          if ((b > 0 && a > INT_MAX - b) || (b < 0 && a < INT_MIN - b))
            return false;
          out = a + b;
          return true;
        }
        static bool subtract(int a, int b, int &out)
        {
          if ((b > 0 && a < INT_MIN + b) || (b < 0 && a > INT_MAX + b))
            return false;
          out = a - b;
          return true;
        }
      };

    } // namespace layout
  } // namespace app
} // namespace loka
#endif
