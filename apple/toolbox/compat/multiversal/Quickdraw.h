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

// These read-only text traps use Ptr in Multiversal's Pascal declarations.
inline short TextWidth(const char *text, short offset, short count)
{
  return TextWidth(const_cast<char *>(text), offset, count);
}

inline void DrawText(const char *text, short offset, short count)
{
  DrawText(const_cast<char *>(text), offset, count);
}

inline void MeasureText(short count, const char *text, short *positions)
{
  MeasureText(count, const_cast<char *>(text), reinterpret_cast<char *>(positions));
}

#endif
