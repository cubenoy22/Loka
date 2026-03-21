#include "ToolboxProfiler.hpp"
#include "loka/core/Profiler.hpp"
#include <Timer.h>

// Backend implementation using TickCount()
static long ToolboxGetTicks()
{
  return TickCount();
}

static loka::core::ProfilerBackend sToolboxBackend = {
    &ToolboxGetTicks};

void InitToolboxProfiler()
{
  loka::core::gProfilerBackend = &sToolboxBackend;
}
