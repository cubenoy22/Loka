#ifndef LOKA_QUICKJS_PROBE_MEMORY_H
#define LOKA_QUICKJS_PROBE_MEMORY_H
#include "quickjs.h"

/** Probe-local allocator; one runtime at a time, on the main thread. */
JSRuntime *probe_new_runtime(void);
/** Print allocator payloads and a process-heap snapshot; return live payload bytes. */
size_t probe_memory_report(const char *phase);
#endif
