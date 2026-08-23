#ifndef LOKA_WIN32_PROFILER_HPP
#define LOKA_WIN32_PROFILER_HPP

namespace loka
{
  namespace platform
  {
    /** Converts a performance-counter reading to wrapping microsecond ticks. */
    long Win32ProfileTicksFromCounter(unsigned long long counter, unsigned long long frequency);
  } // namespace platform
} // namespace loka

void InitWin32Profiler();

#endif // LOKA_WIN32_PROFILER_HPP
