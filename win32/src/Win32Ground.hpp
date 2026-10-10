#ifndef LOKA_WIN32_GROUND_HPP
#define LOKA_WIN32_GROUND_HPP

#include "app/style/SurfaceGround.hpp"
#include <windows.h>
#include <cassert>

namespace loka
{
  namespace win32
  {
    // Deliberate platform-seam twins of ToolboxGround and MacGround.
    /** Answers system color indices so resolution remains live. Transparent and
        native grounds decline without changing out. */
    inline bool QueryWin32GroundColor(app::SurfaceGround ground, int &out)
    {
      switch (ground)
      {
      case app::SURFACE_GROUND_TRANSPARENT:
      case app::SURFACE_GROUND_NATIVE:
        return false;
      case app::SURFACE_GROUND_WINDOW:
        out = COLOR_BTNFACE;
        return true;
      case app::SURFACE_GROUND_DOCUMENT:
        out = COLOR_WINDOW;
        return true;
      case app::SURFACE_GROUND_CONTROL:
        out = COLOR_BTNFACE;
        return true;
      }
      return false;
    }

    /** Standard palette text row paired with the answering ground beneath it.
        Declining grounds assert in debug and return no system index (-1) in
        release; callers must supply the ground actually painted underneath. */
    inline int Win32TextRoleColor(app::SurfaceGround ground)
    {
      switch (ground)
      {
      case app::SURFACE_GROUND_WINDOW:
      case app::SURFACE_GROUND_CONTROL:
        return COLOR_BTNTEXT;
      case app::SURFACE_GROUND_DOCUMENT:
        return COLOR_WINDOWTEXT;
      case app::SURFACE_GROUND_TRANSPARENT:
      case app::SURFACE_GROUND_NATIVE:
        break;
      }
      assert(false && "text requires an answering ground");
      return -1;
    }
  }
}

#endif
