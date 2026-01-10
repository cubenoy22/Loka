#include "ToolboxProfiler.hpp"
#include <Timer.h>
#include <cstdio>

// Toolbox-specific result string
std::string gProfileResultString;

// Backend implementation using TickCount()
static long ToolboxGetTicks()
{
  return TickCount();
}

static declara::core::ProfilerBackend sToolboxBackend = {
    &ToolboxGetTicks};

void InitToolboxProfiler()
{
  declara::core::gProfilerBackend = &sToolboxBackend;
}

void BuildProfileResultString()
{
  gProfileResultString.clear();
  for (int i = 0; i < gProfileCount; i++)
  {
    char part[48];
    std::sprintf(part, "%s:%ld ", gProfileData[i].name, gProfileData[i].ticks);
    gProfileResultString += part;
  }
  gProfileCount = 0;
}
