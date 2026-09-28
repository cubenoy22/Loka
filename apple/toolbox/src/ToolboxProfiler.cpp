#include "ToolboxProfiler.hpp"
#include "core/Profiler.hpp"
#include <Timer.h>

#if LOKA_PROFILE_FUNC_TICKS || LOKA_TOOLBOX_PASS_TIMING
#if defined(LOKA_TOOLBOX_MULTIVERSAL_INTERFACES)
// Multiversal omits Microseconds. The trap writes two consecutive 32-bit words;
// use stack storage with the same ABI as Universal's UnsignedWide.
extern "C" pascal void ToolboxMicroseconds(unsigned long *words) M68K_INLINE(0xA193, 0x225F, 0x22C8, 0x2280);
#endif

unsigned long ToolboxProfileMicroseconds()
{
#if defined(LOKA_TOOLBOX_MULTIVERSAL_INTERFACES)
  unsigned long words[2];
  ToolboxMicroseconds(words);
  return words[1];
#else
  UnsignedWide now;
  Microseconds(&now);
  return now.lo;
#endif
}

#endif // LOKA_PROFILE_FUNC_TICKS || LOKA_TOOLBOX_PASS_TIMING

// Backend implementation using TickCount()
static long ToolboxGetTicks()
{
  return TickCount();
}

static loka::core::ProfilerBackend sToolboxBackend = {&ToolboxGetTicks};

void InitToolboxProfiler()
{
  loka::core::gProfilerBackend = &sToolboxBackend;
}
