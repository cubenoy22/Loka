#include <MacMemory.h>
#include "memory.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

/** Process-local measurement owner. Only allocator operations update these facts. */
static struct
{
  size_t live;
  size_t peak;
} allocation;

static size_t usable(const void *pointer)
{
  return pointer ? (size_t)GetPtrSize((Ptr)pointer) : 0;
}

static void *allocate(void *opaque, size_t size)
{
  Ptr pointer;
  (void)opaque;
  if (size > LONG_MAX)
    return NULL;
  pointer = NewPtr((Size)size);
  if (pointer)
  {
    allocation.live += usable(pointer);
    if (allocation.live > allocation.peak)
      allocation.peak = allocation.live;
  }
  return pointer;
}

static void release(void *opaque, void *pointer)
{
  (void)opaque;
  if (!pointer)
    return;
  allocation.live -= usable(pointer);
  DisposePtr((Ptr)pointer);
}

static void *allocate_zero(void *opaque, size_t count, size_t size)
{
  void *pointer;
  if (size && count > (size_t)-1 / size)
    return NULL;
  pointer = allocate(opaque, count * size);
  if (pointer)
    memset(pointer, 0, count * size);
  return pointer;
}

static void *resize(void *opaque, void *pointer, size_t size)
{
  void *next;
  size_t old_size;
  if (!size)
  {
    release(opaque, pointer);
    return NULL;
  }
  if (!pointer)
    return allocate(opaque, size);
  old_size = usable(pointer);
  next = allocate(opaque, size);
  if (!next)
    return NULL;
  memcpy(next, pointer, old_size < size ? old_size : size);
  release(opaque, pointer);
  return next;
}

JSRuntime *probe_new_runtime(void)
{
  static const JSMallocFunctions functions = {allocate_zero, allocate, release, resize, usable};
  assert(allocation.live == 0);
  allocation.peak = 0;
  return JS_NewRuntime2(&functions, NULL);
}

size_t probe_memory_report(const char *phase)
{
  printf("%s: payload_live=%lu payload_peak=%lu heap_free=%ld\n",
         phase,
         (unsigned long)allocation.live,
         (unsigned long)allocation.peak,
         (long)FreeMem());
  return allocation.live;
}
