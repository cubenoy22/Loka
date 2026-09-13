#ifndef LOKA_TEST_CLASSIC_MAC_MEMORY_H
#define LOKA_TEST_CLASSIC_MAC_MEMORY_H

#include <stdint.h>

// Host-only substitute for the two Memory Manager calls used by the source.
typedef char *Ptr;
typedef int32_t Size;
Ptr NewPtr(Size size);
void DisposePtr(Ptr storage);

#endif
