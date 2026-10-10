#ifndef LOKA_MAC_GROUND_HPP
#define LOKA_MAC_GROUND_HPP

#include "app/style/SurfaceGround.hpp"
#import <AppKit/AppKit.h>

namespace loka
{
  namespace macos
  {
    // Deliberate platform-seam twins of ToolboxGround/ToolboxPaintGround.
    /** Answers dynamic system colors; transparent/native decline with out untouched.
        AppKit paints the window ground, but the table also exposes it for contrast. */
    bool QueryMacGroundColor(app::SurfaceGround ground, NSColor *&out);

    /** Standard palette text row, resolved by AppKit in the drawing appearance. */
    NSColor *MacTextRoleColor();

    /** Paints an answering ground. Declining roles assert in debug and paint
        nothing in release. */
    void MacPaintGround(app::SurfaceGround ground, NSRect rect);
  }
}

#endif
