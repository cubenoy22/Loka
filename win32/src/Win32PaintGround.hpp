#ifndef LOKA_WIN32_PAINT_GROUND_HPP
#define LOKA_WIN32_PAINT_GROUND_HPP

#include "Win32Ground.hpp"

namespace loka
{
  namespace win32
  {
    // Deliberate platform-seam twin of ToolboxPaintGround/MacPaintGround.
    /** Paints with a system-owned brush (never deleted). Declining roles assert
        in debug and paint nothing in release. Returns paint success so a
        RectSurface certifies presentation only after every fill succeeds. */
    inline bool Win32PaintGround(HDC dc, app::SurfaceGround ground, const RECT &rect)
    {
      int index;
      const bool answered = QueryWin32GroundColor(ground, index);
      assert(answered && "only Loka-owned grounds may be painted");
      return answered && FillRect(dc, &rect, GetSysColorBrush(index)) != 0;
    }

    /** Shared root/viewport WM_CTLCOLORSTATIC response for native static text
        on the WINDOW ground. A STATIC stays transparent so a ZStack Text
        overlaps the sibling it sits on (HelloWorld's decoration); the parent
        paints the ground under it whenever the parent itself is invalidated.
        A surface tick no longer invalidates the root, which is what removed
        the per-tick flicker. */
    inline HBRUSH Win32StaticTextColors(HDC dc)
    {
      if (dc)
      {
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, GetSysColor(Win32TextRoleColor(app::SURFACE_GROUND_WINDOW)));
      }
      return static_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
    }
  }
}

#endif
