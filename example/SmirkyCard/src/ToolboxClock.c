#include <stdint.h>
#if defined(LOKA_TOOLBOX_MULTIVERSAL_INTERFACES)
#include <OSUtils.h>

uint64_t smirkycard_hrtime_ns(void)
{
  // Multiversal has no Microseconds: resolution is 1/60 s (16,666,667 ns).
  // This clock feeds QuickJS os.now/performance timing, never layout.
  return (uint64_t)TickCount() * 16666667;
}
#else
#include <Timer.h>

uint64_t smirkycard_hrtime_ns(void)
{
  UnsignedWide ticks;
  Microseconds(&ticks);
  return (((uint64_t)ticks.hi << 32) | ticks.lo) * 1000;
}
#endif
