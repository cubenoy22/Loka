#include "platform/Win32Profiler.hpp"
#include "core/Profiler.hpp"
#include <windows.h>

namespace
{
  static long Win32ProfileTicks()
  {
    static LARGE_INTEGER sFrequency = {0};
    LARGE_INTEGER counter;
    if (sFrequency.QuadPart == 0)
    {
      QueryPerformanceFrequency(&sFrequency);
    }
    QueryPerformanceCounter(&counter);
    if (sFrequency.QuadPart == 0)
    {
      return 0;
    }
    return static_cast<long>((counter.QuadPart * 1000000LL) / sFrequency.QuadPart);
  }

  static loka::core::ProfilerBackend sWin32Backend = {
    &Win32ProfileTicks
  };
}

void InitWin32Profiler()
{
  loka::core::gProfilerBackend = &sWin32Backend;
}
