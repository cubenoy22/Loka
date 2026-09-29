#ifndef LOKA_APP_LAYOUT_CONTROL_WIDTH_HPP
#define LOKA_APP_LAYOUT_CONTROL_WIDTH_HPP

namespace loka
{
  namespace app
  {
    namespace layout
    {
      /** A positive container offer determines the width; otherwise use the
          control's natural width. */
      inline int offeredOrNaturalWidth(int offeredWidth, int naturalWidth)
      {
        return offeredWidth > 0 ? offeredWidth : naturalWidth;
      }
    } // namespace layout
  } // namespace app
} // namespace loka

#endif // LOKA_APP_LAYOUT_CONTROL_WIDTH_HPP
