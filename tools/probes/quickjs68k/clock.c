#include <stdint.h>
#include <Timer.h>

uint64_t loka_probe_hrtime_ns(void)
{
  UnsignedWide ticks;
  Microseconds(&ticks);
  return (((uint64_t)ticks.hi << 32) | ticks.lo) * 1000;
}
