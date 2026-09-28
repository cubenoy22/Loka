#ifndef LOKA_APP_LAYOUT_LAZY_LAYOUT_HPP
#define LOKA_APP_LAYOUT_LAZY_LAYOUT_HPP

#include <climits>
#include <cstddef>
#include "app/nodes/nestable/RowColumn.hpp"
#include "core/Frame.hpp"

namespace loka
{
  namespace app
  {
    namespace layout
    {
      /** A completed half-open interval in declaration order. */
      struct LazyWindow
      {
        unsigned first;
        unsigned count;
      };

      enum LazyExtentStatus
      {
        LAZY_EXTENT_READY,
        LAZY_EXTENT_INVALID_INPUT,
        LAZY_EXTENT_INT_RANGE_REFUSED
      };

      /** Extent and refusal travel together; an empty list has a valid 0x0 extent. */
      struct LazyExtent
      {
        LazyExtentStatus status;
        loka::core::Frame frame;
      };

      /** Pointer-free placement policy snapshot, copied by value with props.
          itemCount bounds selection; extent(count) is pure and never changes it.
          FixedGrid selects whole main-axis rows with half-open viewport edges.
          Canvas retains its separate conservative far-edge traversal. Extent and
          placement accept size_t to preserve Canvas counts on wider hosts. */
      struct LazyLayout
      {
        enum Kind
        {
          FIXED_GRID
        };
        Kind kind;
        StackAxis axis;
        short cellWidth;
        short cellHeight;
        unsigned short wrap;
        unsigned short margin;
        unsigned itemCount;

        LazyExtent extent(size_t count) const
        {
          LazyExtent result = {LAZY_EXTENT_INVALID_INPUT, loka::core::Frame()};
          switch (this->kind)
          {
          case FIXED_GRID:
            if (!this->validGrid())
              return result;
            const size_t rows = count / this->wrap + (count % this->wrap != 0);
            const size_t columns = count < this->wrap ? count : this->wrap;
            const size_t x = this->axis == STACK_AXIS_COLUMN ? columns : rows;
            const size_t y = this->axis == STACK_AXIS_COLUMN ? rows : columns;
            if (x > static_cast<unsigned>(INT_MAX / this->cellWidth)
                || y > static_cast<unsigned>(INT_MAX / this->cellHeight))
            {
              result.status = LAZY_EXTENT_INT_RANGE_REFUSED;
              return result;
            }
            result.status = LAZY_EXTENT_READY;
            result.frame = loka::core::Frame(0, 0, x * this->cellWidth, y * this->cellHeight);
            return result;
          }
          return result;
        }

        loka::core::Frame place(size_t index) const
        {
          switch (this->kind)
          {
          case FIXED_GRID:
            if (!this->validGrid())
              return loka::core::Frame();
            const size_t x = this->axis == STACK_AXIS_COLUMN ? index % this->wrap : index / this->wrap;
            const size_t y = this->axis == STACK_AXIS_COLUMN ? index / this->wrap : index % this->wrap;
            if (x > static_cast<unsigned>(INT_MAX / this->cellWidth)
                || y > static_cast<unsigned>(INT_MAX / this->cellHeight))
              return loka::core::Frame();
            return loka::core::Frame(x * this->cellWidth, y * this->cellHeight, this->cellWidth, this->cellHeight);
          }
          return loka::core::Frame();
        }

        LazyWindow indicesIn(const loka::core::Frame &viewport) const
        {
          LazyWindow empty = {0, 0};
          switch (this->kind)
          {
          case FIXED_GRID:
            const LazyExtent content = this->extent(this->itemCount);
            if (!viewport.hasSize() || content.status != LAZY_EXTENT_READY || !this->itemCount)
              return empty;
            const bool vertical = this->axis == STACK_AXIS_COLUMN;
            const int cross = vertical ? viewport.x : viewport.y;
            const int crossSize = vertical ? viewport.width : viewport.height;
            const int crossExtent = vertical ? content.frame.width : content.frame.height;
            if (cross >= crossExtent
                || (cross < 0 && 0u - static_cast<unsigned>(cross) >= static_cast<unsigned>(crossSize)))
              return empty;
            const int origin = vertical ? viewport.y : viewport.x;
            const int size = vertical ? viewport.height : viewport.width;
            const unsigned cell = vertical ? this->cellHeight : this->cellWidth;
            // Positive sizes make the upper bound the only possible addition overflow.
            if (origin > INT_MAX - size)
              return empty;
            const int end = origin + size;
            if (end <= 0)
              return empty;
            const unsigned rows = this->itemCount / this->wrap + (this->itemCount % this->wrap != 0);
            unsigned near = origin > 0 ? static_cast<unsigned>(origin) / cell : 0;
            unsigned far = static_cast<unsigned>(end) / cell + (static_cast<unsigned>(end) % cell != 0);
            near = near > this->margin ? near - this->margin : 0;
            if (near >= rows)
              return empty;
            far = far >= rows || this->margin >= rows - far ? rows : far + this->margin;
            const unsigned first = near * this->wrap;
            const unsigned last = far == rows ? this->itemCount : far * this->wrap;
            LazyWindow result = {first, last - first};
            return result;
          }
          return empty;
        }

      private:
        bool validGrid() const
        {
          return this->wrap && this->cellWidth > 0 && this->cellHeight > 0
                 && (this->axis == STACK_AXIS_COLUMN || this->axis == STACK_AXIS_ROW);
        }
      };

      /** Construct a completed policy for a list snapshot. Margin stays zero in PR 1. */
      inline LazyLayout FixedGrid(short width,
                                  short height,
                                  unsigned short wrap = 1,
                                  unsigned itemCount = 0,
                                  StackAxis axis = STACK_AXIS_COLUMN,
                                  unsigned short margin = 0)
      {
        LazyLayout value = {LazyLayout::FIXED_GRID, axis, width, height, wrap, margin, itemCount};
        return value;
      }
    } // namespace layout
  } // namespace app
} // namespace loka
#endif
