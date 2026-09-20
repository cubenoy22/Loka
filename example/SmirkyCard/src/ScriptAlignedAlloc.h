#ifndef SMIRKY_SCRIPT_ALIGNED_ALLOC_H
#define SMIRKY_SCRIPT_ALIGNED_ALLOC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** QuickJS tags pointers in their low two bits (realm_and_id) and lays its
      arenas out with 8-byte members, so every block it receives must be at
      least 8-byte aligned. Retro68's malloc is NewPtr, and the Mac Plus
      Memory Manager hands out 2-byte-aligned blocks (#837), so the engine
      must align on top of whatever the C library gives it. The base pair is a
      parameter so a headless pin can drive the wrapper with a 2-byte-aligned
      source. */
  typedef struct SmirkyBaseAllocator
  {
    void *(*allocate)(size_t size);
    void (*release)(void *block);
  } SmirkyBaseAllocator;

  enum
  {
    SMIRKY_SCRIPT_ALLOC_ALIGN = 8
  };

  void *SmirkyAlignedAllocate(const SmirkyBaseAllocator *base, size_t size);
  void SmirkyAlignedRelease(const SmirkyBaseAllocator *base, void *block);
  void *SmirkyAlignedReallocate(const SmirkyBaseAllocator *base, void *block, size_t size);
  size_t SmirkyAlignedUsableSize(const void *block);

#ifdef __cplusplus
}
#endif
#endif
