#include "MacDisplayAppearance.hpp"
#include <Foundation/Foundation.h>

namespace loka
{
  namespace macos
  {
    bool TryReadDisplayAppearance(void *appearanceObject, Window::DisplayAppearance &out)
    {
      id appearance = (id)appearanceObject;
      SEL matchingSelector = @selector(bestMatchFromAppearancesWithNames:);
      if (!appearance || ![appearance respondsToSelector:matchingSelector])
      {
        return false;
      }

      // bestMatchFromAppearancesWithNames: arrived with the light/dark
      // appearance system in 10.14. Its presence is the capability being
      // queried; effectiveAppearance alone dates back to 10.9 and cannot make
      // the pre-Mojave distinction promised by this seam.
      NSArray *candidates = [NSArray arrayWithObjects:@"NSAppearanceNameAqua",
                                                       @"NSAppearanceNameDarkAqua",
                                                       nil];
      NSString *name = (NSString *)[appearance performSelector:matchingSelector withObject:candidates];
      if ([name isEqualToString:@"NSAppearanceNameDarkAqua"])
      {
        out = Window::DISPLAY_APPEARANCE_DARK;
        return true;
      }
      if ([name isEqualToString:@"NSAppearanceNameAqua"])
      {
        out = Window::DISPLAY_APPEARANCE_LIGHT;
        return true;
      }
      return false;
    }
  } // namespace macos
} // namespace loka
