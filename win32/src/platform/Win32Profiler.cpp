#include "platform/Win32Profiler.hpp"
#include "core/Profiler.hpp"
#include <windows.h>

namespace
{
  static unsigned long ScaleCounterRemainder(unsigned long long remainder, unsigned long long frequency)
  {
    const unsigned long multiplier = 1000000UL;
    const unsigned long long maximum = ~0ULL;
    if (remainder <= maximum / multiplier)
    {
      return static_cast<unsigned long>((remainder * multiplier) / frequency);
    }
    // QPC frequencies take the exact integer path above. Keep even an
    // implausibly large injected frequency defined without growing the hot
    // path into a general-purpose wide multiply/divide implementation.
    const long double scaled = static_cast<long double>(remainder) * multiplier / frequency;
    const unsigned long rounded = static_cast<unsigned long>(scaled);
    return rounded < multiplier ? rounded : multiplier - 1UL;
  }

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
    return loka::platform::Win32ProfileTicksFromCounter(static_cast<unsigned long long>(counter.QuadPart),
                                                        static_cast<unsigned long long>(sFrequency.QuadPart));
  }

  static loka::core::ProfilerBackend sWin32Backend = {&Win32ProfileTicks};
} // namespace

long loka::platform::Win32ProfileTicksFromCounter(unsigned long long counter, unsigned long long frequency)
{
  if (frequency == 0)
  {
    return 0;
  }
  const unsigned long wholeMicroseconds = static_cast<unsigned long>(counter / frequency) * 1000000UL;
  const unsigned long fractionalMicroseconds = ScaleCounterRemainder(counter % frequency, frequency);
  return static_cast<long>(wholeMicroseconds + fractionalMicroseconds);
}

void InitWin32Profiler()
{
  loka::core::gProfilerBackend = &sWin32Backend;
}
