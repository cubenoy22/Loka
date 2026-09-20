#include "ScriptAlignedAlloc.h"
#include <stdint.h>
#include <string.h>

/* Sits immediately before the aligned user block. */
typedef struct SmirkyAlignedHeader
{
  void *raw;
  size_t size;
} SmirkyAlignedHeader;

static SmirkyAlignedHeader *HeaderOf(void *block)
{
  return (SmirkyAlignedHeader *)((char *)block - sizeof(SmirkyAlignedHeader));
}

void *SmirkyAlignedAllocate(const SmirkyBaseAllocator *base, size_t size)
{
  const size_t slack = sizeof(SmirkyAlignedHeader) + SMIRKY_SCRIPT_ALLOC_ALIGN - 1;
  char *raw;
  uintptr_t user;
  SmirkyAlignedHeader *header;
  if (size > (size_t)-1 - slack)
    return NULL;
  raw = (char *)base->allocate(size + slack);
  if (!raw)
    return NULL;
  user = ((uintptr_t)raw + sizeof(SmirkyAlignedHeader) + (SMIRKY_SCRIPT_ALLOC_ALIGN - 1))
         & ~(uintptr_t)(SMIRKY_SCRIPT_ALLOC_ALIGN - 1);
  header = HeaderOf((void *)user);
  header->raw = raw;
  header->size = size;
  return (void *)user;
}

void SmirkyAlignedRelease(const SmirkyBaseAllocator *base, void *block)
{
  if (block)
    base->release(HeaderOf(block)->raw);
}

void *SmirkyAlignedReallocate(const SmirkyBaseAllocator *base, void *block, size_t size)
{
  void *replacement;
  size_t old;
  if (!block)
    return SmirkyAlignedAllocate(base, size);
  if (size == 0)
  {
    SmirkyAlignedRelease(base, block);
    return NULL;
  }
  old = HeaderOf(block)->size;
  if (size <= old)
  {
    HeaderOf(block)->size = size;
    return block;
  }
  replacement = SmirkyAlignedAllocate(base, size);
  if (!replacement)
    return NULL;
  memcpy(replacement, block, old);
  SmirkyAlignedRelease(base, block);
  return replacement;
}

size_t SmirkyAlignedUsableSize(const void *block)
{
  return block ? HeaderOf((void *)block)->size : 0;
}
