#include "MacGround.hpp"
#include <AvailabilityMacros.h>
#include <cassert>

namespace loka
{
  namespace macos
  {
    bool QueryMacGroundColor(app::SurfaceGround ground, NSColor *&out)
    {
      switch (ground)
      {
      case app::SURFACE_GROUND_TRANSPARENT:
      case app::SURFACE_GROUND_NATIVE:
        return false;
      case app::SURFACE_GROUND_WINDOW:
        out = [NSColor windowBackgroundColor];
        return true;
      case app::SURFACE_GROUND_DOCUMENT:
        out = [NSColor textBackgroundColor];
        return true;
      case app::SURFACE_GROUND_CONTROL:
        out = [NSColor controlBackgroundColor];
        return true;
      }
      return false;
    }

    NSColor *MacTextRoleColor()
    {
#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && MAC_OS_X_VERSION_MAX_ALLOWED >= 101000
      if ([NSColor respondsToSelector:@selector(labelColor)])
        return [NSColor labelColor];
#endif
      return [NSColor textColor];
    }

    void MacPaintGround(app::SurfaceGround ground, NSRect rect)
    {
      NSColor *color = nil;
      const bool answered = QueryMacGroundColor(ground, color);
      assert(answered && "only Loka-owned grounds may be painted");
      if (!answered)
        return;
      [color setFill];
      NSRectFill(rect);
    }
  }
}
