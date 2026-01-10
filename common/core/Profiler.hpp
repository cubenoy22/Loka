#ifndef DECLARA_CORE_PROFILER_HPP
#define DECLARA_CORE_PROFILER_HPP

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

    // Cumulative timing for compose phases
    extern long gComposeAttachTicks;
    extern long gComposeNodeTicks;
    extern long gComposeCreateTicks;

    // Cumulative timing for composeTree breakdown
    extern long gTreeVirtTicks;   // virtual calls (asBoundary, asComposable, asNestable)
    extern long gTreeCtxTicks;    // ComponentContext setup
    extern long gTreeCompTicks;   // compose() calls
    extern int gTreeNodeCount;    // number of nodes traversed

    // Record cumulative compose timings to profile data
    inline void RecordComposeTicks()
    {
      if (gProfileCount < 24)
      {
        gProfileData[gProfileCount].name = "cAtch";
        gProfileData[gProfileCount].ticks = gComposeAttachTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "cNode";
        gProfileData[gProfileCount].ticks = gComposeNodeTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "cCrea";
        gProfileData[gProfileCount].ticks = gComposeCreateTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "tVirt";
        gProfileData[gProfileCount].ticks = gTreeVirtTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "tCtx";
        gProfileData[gProfileCount].ticks = gTreeCtxTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "tComp";
        gProfileData[gProfileCount].ticks = gTreeCompTicks;
        gProfileCount++;
        gProfileData[gProfileCount].name = "nodes";
        gProfileData[gProfileCount].ticks = gTreeNodeCount;
        gProfileCount++;
      }
      gComposeAttachTicks = 0;
      gComposeNodeTicks = 0;
      gComposeCreateTicks = 0;
      gTreeVirtTicks = 0;
      gTreeCtxTicks = 0;
      gTreeCompTicks = 0;
      gTreeNodeCount = 0;
    }

  } // namespace core
} // namespace declara

#endif // DECLARA_CORE_PROFILER_HPP
