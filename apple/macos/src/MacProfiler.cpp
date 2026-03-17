#include "MacProfiler.hpp"
#include "loka/core/Profiler.hpp"
#include <mach/mach_time.h>

namespace
{
  static long MacProfileTicks()
  {
    static mach_timebase_info_data_t sTimebase = {0, 0};
    if (sTimebase.denom == 0)
    {
      mach_timebase_info(&sTimebase);
    }
    const unsigned long long ticks = mach_absolute_time();
    const unsigned long long nanos = (ticks * static_cast<unsigned long long>(sTimebase.numer)) /
                                     static_cast<unsigned long long>(sTimebase.denom);
    return static_cast<long>(nanos / 1000ULL);
  }

  static loka::core::ProfilerBackend sMacBackend = {
    &MacProfileTicks
  };
}

void InitMacProfiler()
{
  loka::core::gProfilerBackend = &sMacBackend;
}
