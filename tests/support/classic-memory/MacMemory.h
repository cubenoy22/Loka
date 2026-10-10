#ifndef LOKA_TEST_CLASSIC_MAC_MEMORY_H
#define LOKA_TEST_CLASSIC_MAC_MEMORY_H

#include <stdint.h>

// Host-only Memory Manager seam used by the source.
typedef char *Ptr;
typedef int32_t Size;
typedef long (*GrowZoneUPP)(Size);
struct Zone { GrowZoneUPP gzProc; };
Zone *GetZone();
void SetGrowZone(GrowZoneUPP);
Ptr NewPtr(Size size);
void DisposePtr(Ptr storage);

#endif
