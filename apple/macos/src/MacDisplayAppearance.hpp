#ifndef LOKA_MAC_DISPLAY_APPEARANCE_HPP
#define LOKA_MAC_DISPLAY_APPEARANCE_HPP

#include "app/core/Window.hpp"

namespace loka
{
  namespace macos
  {
    /** Reads the light/dark fact from a borrowed native NSAppearance object.
        Presence is derived from the native object's capabilities; `out` is
        untouched when the object cannot provide a definite answer. */
    bool TryReadDisplayAppearance(void *appearanceObject, Window::DisplayAppearance &out);
  } // namespace macos
} // namespace loka

#endif // LOKA_MAC_DISPLAY_APPEARANCE_HPP
