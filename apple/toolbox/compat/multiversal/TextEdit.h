#ifndef LOKA_TOOLBOX_MULTIVERSAL_TEXT_EDIT_H
#define LOKA_TOOLBOX_MULTIVERSAL_TEXT_EDIT_H

#include <Multiverse.h>

// Multiversal omits Universal Interfaces' const overload.
inline void TESetText(const void *text, long length, TEHandle handle)
{
  TESetText(const_cast<char *>(static_cast<const char *>(text)), length, handle);
}

#endif
