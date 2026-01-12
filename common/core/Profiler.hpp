#ifndef DECLARA_CORE_PROFILER_HPP
#define DECLARA_CORE_PROFILER_HPP

#include <cassert>
#include <cstdio>

namespace declara
{
  namespace core
  {
    // Platform-specific backend interface
    struct ProfilerBackend
    {
      long (*getTicks)();
    };

    // Set by platform code at startup
    extern ProfilerBackend *gProfilerBackend;

    // Get current ticks (0 if no backend registered)
    inline long ProfileTicks()
    {
      return gProfilerBackend ? gProfilerBackend->getTicks() : 0;
    }

    // Profile entry storage
    struct ProfileEntry
    {
      const char *name;
      long ticks;
    };

    // Shared profile data (max 32 entries)
    extern ProfileEntry gProfileData[32];
    extern int gProfileCount;

    // Scoped profiler - records duration on destruction
    struct ScopedProfile
    {
      const char *name_;
      long start_;

      ScopedProfile(const char *name)
          : name_(name), start_(ProfileTicks()) {}

      ~ScopedProfile()
      {
        if (gProfileCount < 32)
        {
          gProfileData[gProfileCount].name = name_;
          gProfileData[gProfileCount].ticks = ProfileTicks() - start_;
          gProfileCount++;
        }
      }
    };

    // Manual checkpoint marker
    inline void ProfileCheckpoint(const char *name)
    {
      if (gProfileCount < 32)
      {
        gProfileData[gProfileCount].name = name;
        gProfileData[gProfileCount].ticks = ProfileTicks();
        gProfileCount++;
      }
    }

    // ============================================================
    // Function-level profiler (static slot registration)
    // ============================================================
#ifndef LOKA_PROFILE_FUNC_TICKS
#define LOKA_PROFILE_FUNC_TICKS 0
#endif

    struct FuncProfileSlot
    {
      const char *file;
      const char *func;
      int line;
      int callCount;
      long totalTicks;
    };

    enum { kMaxProfileSlots = 256 };
    extern FuncProfileSlot *gProfileSlots[kMaxProfileSlots];
    extern int gProfileSlotCount;

    // Register slot (called once per static slot)
    inline int RegisterProfileSlot(FuncProfileSlot *slot)
    {
      assert(gProfileSlotCount < kMaxProfileSlots && "Profile slot overflow");
      gProfileSlots[gProfileSlotCount] = slot;
      return gProfileSlotCount++;
    }

    struct FuncProfileScope
    {
      FuncProfileSlot *slot_;
      long start_;

      FuncProfileScope(FuncProfileSlot *slot)
          : slot_(slot), start_(ProfileTicks())
      {
        ++slot_->callCount;
      }

      ~FuncProfileScope()
      {
        slot_->totalTicks += ProfileTicks() - start_;
      }
    };

    inline void ResetFuncProfile()
    {
      for (int i = 0; i < gProfileSlotCount; ++i)
      {
        gProfileSlots[i]->callCount = 0;
        gProfileSlots[i]->totalTicks = 0;
      }
    }

    // Sort slots by totalTicks descending
    inline void SortProfileSlotsByTicks()
    {
      for (int i = 0; i < gProfileSlotCount - 1; ++i)
      {
        for (int j = i + 1; j < gProfileSlotCount; ++j)
        {
          if (gProfileSlots[j]->totalTicks > gProfileSlots[i]->totalTicks)
          {
            FuncProfileSlot *tmp = gProfileSlots[i];
            gProfileSlots[i] = gProfileSlots[j];
            gProfileSlots[j] = tmp;
          }
        }
      }
    }

    // Stream output interface
    struct ProfileOutputStream
    {
      virtual ~ProfileOutputStream() {}
      virtual void write(const char *data, int len) = 0;
    };

    // Dump to stream (no full memory allocation)
    // Uses \r for Classic Mac line endings
    inline void DumpFuncProfileToStream(ProfileOutputStream &out)
    {
      SortProfileSlotsByTicks();
      char buf[256];
      for (int i = 0; i < gProfileSlotCount; ++i)
      {
        FuncProfileSlot *s = gProfileSlots[i];
        if (s->callCount == 0) continue;
        int len = snprintf(buf, sizeof(buf), "%s:%d\t%s\t%d\t%ld\r",
                           s->file, s->line, s->func, s->callCount, s->totalTicks);
        out.write(buf, len);
      }
    }

#if LOKA_PROFILE_FUNC_TICKS
#define PROFILE_FUNC() \
  static ::declara::core::FuncProfileSlot _pslot_ = {__FILE__, __func__, __LINE__, 0, 0}; \
  static int _pslot_reg_ = ::declara::core::RegisterProfileSlot(&_pslot_); \
  ::declara::core::FuncProfileScope _pscope_(&_pslot_)
#define PROFILE_SECTION(name) \
  static ::declara::core::FuncProfileSlot _psec_##__LINE__ = {__FILE__, name, __LINE__, 0, 0}; \
  static int _psec_reg_##__LINE__ = ::declara::core::RegisterProfileSlot(&_psec_##__LINE__); \
  ::declara::core::FuncProfileScope _pscope_##__LINE__(&_psec_##__LINE__)
#else
#define PROFILE_FUNC()
#define PROFILE_SECTION(name)
#endif

  } // namespace core
} // namespace declara

#endif // DECLARA_CORE_PROFILER_HPP
