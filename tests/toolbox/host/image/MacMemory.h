#ifndef LOKA_TEST_IMAGE_MACMEMORY_H
#define LOKA_TEST_IMAGE_MACMEMORY_H
// Heap admission is unrelated to these decode pins and declines its query.
struct Zone { char *bkLim; };
typedef Zone *THz;
inline long MaxBlock() { return -1; }
inline THz ApplicationZone() { static Zone zone = {0}; return &zone; }
#endif
