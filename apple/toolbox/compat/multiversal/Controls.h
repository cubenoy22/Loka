#ifndef LOKA_TOOLBOX_MULTIVERSAL_CONTROLS_H
#define LOKA_TOOLBOX_MULTIVERSAL_CONTROLS_H

// Multiversal generates Universal-style umbrella headers for the other
// managers Loka uses, but exposes Control Manager declarations only through
// its combined header.
#include <Multiverse.h>

// Universal Interfaces defines ControlRef as the relocatable handle used by
// the classic Control Manager. Multiversal currently aliases it to the
// dereferenced pointer even though its routines still accept ControlHandle.
#define ControlRef ControlHandle

typedef int16_t ControlPartCode;

enum {
  kControlIndicatorPart = 129,
  kControlInactivePart = 255
};

#endif
