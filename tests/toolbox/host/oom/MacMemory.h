#ifndef LOKA_OOM_HOST_MAC_MEMORY_H
#define LOKA_OOM_HOST_MAC_MEMORY_H
#include "Dialogs.h"
typedef long (*GrowZoneUPP)(Size);
struct Zone { GrowZoneUPP gzProc; };
Zone *GetZone();
void SetGrowZone(GrowZoneUPP);
GrowZoneUPP NewGrowZoneUPP(GrowZoneUPP);
#endif
