#ifndef LOKA_TOOLBOX_GROUND_HPP
#define LOKA_TOOLBOX_GROUND_HPP

#include "app/style/SurfaceGround.hpp"

namespace loka
{
  namespace toolbox
  {
    /** QuickDraw planar colors used by the System 7 ground column.
        Appearance/RGB grounds arrive with the color port (#1196 page 4). */
    enum ToolboxPlanarColor
    {
      TOOLBOX_PLANAR_WHITE = 30,
      TOOLBOX_PLANAR_BLACK = 33
    };

    /** Answers only for Loka-owned grounds; transparent/native decline without
        changing out. Basic B&W ports use the System 7 / planar column only. */
    inline bool QueryToolboxGroundColor(app::SurfaceGround ground, ToolboxPlanarColor &out)
    {
      switch (ground)
      {
      case app::SURFACE_GROUND_TRANSPARENT:
      case app::SURFACE_GROUND_NATIVE:
        return false;
      case app::SURFACE_GROUND_WINDOW:
      case app::SURFACE_GROUND_DOCUMENT:
      case app::SURFACE_GROUND_CONTROL:
        out = TOOLBOX_PLANAR_WHITE;
        return true;
      }
      return false;
    }
  }
}

#endif
