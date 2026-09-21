#ifndef LOKA_TESTS_TOOLBOX_PROBE_TIMING_HPP
#define LOKA_TESTS_TOOLBOX_PROBE_TIMING_HPP

#include <cstdio>
#include <Events.h>
#include <Memory.h>
#include <Timer.h>

namespace loka_toolbox_probe
{
#if defined(LOKA_TOOLBOX_MULTIVERSAL_INTERFACES)
  /** Multiversal omits the Microseconds declaration and its two-word result.
      Probe-local ABI shim mirrors Universal Timer.h's four-word 68K trap;
      keep this out of the production compatibility surface. */
  struct UnsignedWide
  {
    unsigned long hi;
    unsigned long lo;
  };
  extern "C"
  {
    pascal void Microseconds(UnsignedWide *microTickCount) M68K_INLINE(0xA193, 0x225F, 0x22C8, 0x2280);
  }
#endif

  /** Stack timer: unsigned subtraction handles a low-word wrap (<71 minutes). */
  struct Timer
  {
    const unsigned long ticks;
    UnsignedWide start;
    Timer()
        : ticks(TickCount())
    {
      Microseconds(&this->start);
    }
    unsigned long elapsed() const
    {
      UnsignedWide end;
      Microseconds(&end);
      return end.lo - this->start.lo;
    }
  };

  /** Heap queries precede compaction and all report I/O, as in AppRecycle. */
  struct Sample
  {
    const unsigned long us;
    const unsigned long ticks;
    const long freeBytes;
    const long maxBlock;
    const long compactBlock;
    explicit Sample(const Timer &timer)
        : us(timer.elapsed()),
          ticks(TickCount() - timer.ticks),
          freeBytes(FreeMem()),
          maxBlock(MaxBlock()),
          compactBlock(CompactMem(0x7FFFFFFFL))
    {
    }
    void write(std::FILE *log, const char *phase) const
    {
      std::fprintf(log,
                   "phase=%s FreeMem=%ld MaxBlock=%ld CompactMem=%ld us=%lu ticks=%lu",
                   phase,
                   this->freeBytes,
                   this->maxBlock,
                   this->compactBlock,
                   this->us,
                   this->ticks);
    }
  };

} // namespace loka_toolbox_probe

#endif
