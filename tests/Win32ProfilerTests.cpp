#include "Win32ProfilerTests.hpp"

#include "support/TestVerify.hpp"

#include <cstdio>

#include "platform/Win32Profiler.hpp"

void testWin32ProfilerConvertsLongRunningCounterWithoutOverflow()
{
  const unsigned long long frequency = 10000000ULL;
  const unsigned long long elevenDaysInSeconds = 11ULL * 24ULL * 60ULL * 60ULL;
  const unsigned long long counter = frequency * elevenDaysInSeconds;
  const unsigned long expected = static_cast<unsigned long>(elevenDaysInSeconds * 1000000ULL);

  const long ticks = loka::platform::Win32ProfileTicksFromCounter(counter, frequency);
  LOKA_VERIFY(static_cast<unsigned long>(ticks) == expected);

  std::printf("testWin32ProfilerConvertsLongRunningCounterWithoutOverflow passed\n");
}

void testWin32ProfilerConvertsExtremeRemainderWithoutOverflow()
{
  const unsigned long long maximum = ~0ULL;
  const unsigned long long frequency = maximum;
  const unsigned long long counter = frequency - 1ULL;

  const long ticks = loka::platform::Win32ProfileTicksFromCounter(counter, frequency);
  LOKA_VERIFY(static_cast<unsigned long>(ticks) == 999999UL);
  LOKA_VERIFY(loka::platform::Win32ProfileTicksFromCounter(counter, 0) == 0);

  std::printf("testWin32ProfilerConvertsExtremeRemainderWithoutOverflow passed\n");
}
