#ifndef LOKA_TOOLBOX_MULTIVERSAL_QUICKDRAW_H
#define LOKA_TOOLBOX_MULTIVERSAL_QUICKDRAW_H

#include <Multiverse.h>

// Universal Interfaces publishes this classic device-attribute index.
enum {
  screenActive = 15
};

// Multiversal omits Universal Interfaces' const overload.
inline void ClipRect(const Rect *rect)
{
  Rect copy = *rect;
  ClipRect(&copy);
}

#endif
