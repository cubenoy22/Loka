#ifndef LOKA_APP_LAYOUT_ALIGNED_LINE_OFFSET_HPP
#define LOKA_APP_LAYOUT_ALIGNED_LINE_OFFSET_HPP

#include "app/style/Style.hpp"
#include <climits>

namespace loka
{
  namespace app
  {
    /** Physical line offset from the painted width; overflowing lines stay left. */
    inline int AlignedLineOffset(int available, int painted, TextAlign align)
    {
      if (painted > available)
        return 0;
      // Refuse an unrepresentable difference without signed integer overflow.
      if (painted < 0 && available > INT_MAX + painted)
        return 0;
      const int remaining = available - painted;
      switch (align)
      {
      case TEXT_ALIGN_LEFT:
        return 0;
      case TEXT_ALIGN_CENTER:
        return remaining / 2;
      case TEXT_ALIGN_RIGHT:
        return remaining;
      }
      return 0;
    }
  } // namespace app
} // namespace loka
#endif
