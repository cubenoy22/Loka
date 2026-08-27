#ifndef LOKA_TESTS_STANDALONE_PERFORMANCE_HPP
#define LOKA_TESTS_STANDALONE_PERFORMANCE_HPP

#include <string>

#include "platform/file/FileHandle.hpp"

namespace loka
{
  namespace dsl
  {
    class SnapRecord;
  }

  namespace standalone_tests
  {
    /** Retains the first terminal scenario verdict for one measured pass. */
    class StandaloneScenarioVerdict
    {
    public:
      StandaloneScenarioVerdict();

      void begin();
      void observe(const dsl::SnapRecord &record);
      bool refusesCompletedPass() const;

    private:
      enum State
      {
        STATE_NOT_STARTED,
        STATE_AWAITING_TERMINAL,
        STATE_SUCCEEDED,
        STATE_FAILED
      };

      State state_;
    };

    /** Owns one bounded set of complete Standalone Flow pass timings.

        Tick values use the active platform profiler's native unit. Storage is
        fixed for the supported 3-10 passes, and rendering happens only after
        all measured application lifetimes have completed. */
    class StandalonePerformanceSession
    {
    public:
      explicit StandalonePerformanceSession(long runCount);

      bool isValid() const;
      bool isComplete() const;

      /** Records one complete Config/App/Flow lifetime. Profiler wrap is
          accepted when the elapsed interval fits in a positive long. */
      bool recordRun(long startTicks, long endTicks);

      /** Renders only a complete report. Refusal leaves out unchanged. */
      bool buildReport(std::string &out) const;

    private:
      enum
      {
        MIN_RUN_COUNT = 3,
        MAX_RUN_COUNT = 10
      };

      StandalonePerformanceSession(const StandalonePerformanceSession &);
      StandalonePerformanceSession &operator=(const StandalonePerformanceSession &);

      const long runCount_;
      bool valid_;
      long completedRuns_;
      long elapsedByRun_[MAX_RUN_COUNT];
    };

    /** Resolves PERF.TXT beside the current application before measurement. */
    bool ResolveStandalonePerformanceReport(platform::file::FileHandle &out);

    /** Clears any stale report before the first measured pass. */
    bool PrepareStandalonePerformanceReport(const platform::file::FileHandle &file);

    /** Writes one completed performance report through the native file seam. */
    bool WriteStandalonePerformanceReport(const platform::file::FileHandle &file,
                                          const StandalonePerformanceSession &session);
  } // namespace standalone_tests
} // namespace loka

#endif // LOKA_TESTS_STANDALONE_PERFORMANCE_HPP
