#ifndef LOKA_WINDOW_ABILITY_DEBUG_STATS_CONTROL_HPP
#define LOKA_WINDOW_ABILITY_DEBUG_STATS_CONTROL_HPP

namespace loka
{
  namespace app
  {
    class IDebugStatsControl
    {
    public:
      typedef void (*DeferredDumpCompletion)(void *);
      virtual ~IDebugStatsControl() {}
      virtual bool dumpDebugStatsToTimestampedFile() = 0;
      virtual void resetDebugStats() = 0;
      virtual void requestDeferredDebugDump() = 0;
      virtual void requestDeferredDebugDumpWithCompletion(DeferredDumpCompletion completion, void *userData) = 0;
      virtual void flushDeferredDebugDump() = 0;
    };
  }
}

#endif // LOKA_WINDOW_ABILITY_DEBUG_STATS_CONTROL_HPP
