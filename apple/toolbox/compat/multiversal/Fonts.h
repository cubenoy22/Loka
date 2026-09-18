#ifndef LOKA_TOOLBOX_MULTIVERSAL_FONTS_H
#define LOKA_TOOLBOX_MULTIVERSAL_FONTS_H

#include <Multiverse.h>

// Match the Universal Interfaces accessors, including the ROM's 12 pt fallback.
inline short GetDefFontSize()
{
  const short size = LMGetSysFontSiz();
  return size ? size : 12;
}

inline short GetSysFont()
{
  return LMGetSysFontFam();
}

#endif
