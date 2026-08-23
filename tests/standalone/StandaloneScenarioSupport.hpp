#ifndef LOKA_TESTS_STANDALONE_SCENARIO_SUPPORT_HPP
#define LOKA_TESTS_STANDALONE_SCENARIO_SUPPORT_HPP

#include <cstdio>

#include "../scenarios/ScenarioTypes.hpp"
#include "StandalonePerformanceConfig.hpp"
#if LOKA_STANDALONE_PERFORMANCE_RUNS > 0
#include "StandalonePerformance.hpp"
#endif
#include "platform/file/FileHandle.hpp"

class App;
class Window;

namespace loka
{
  namespace standalone_tests
  {
    /** Reports a standalone Window's content-local bounds when available. */
    scenario_tests::CaptureContentBounds StandaloneContentBounds(Window *window);

    /** Resolves the application-side audit file shared by presentation apps. */
    platform::file::FileHandle ResolveStandaloneAuditFile();

    /** Owns the bounded startup wait and terminal App control shared by the
        TEST-only Standalone Flow presentations. A refused MainNode mount
        produces one diagnostic, requests shutdown, and remains terminal. */
    class StandaloneRunControl
    {
    public:
      enum Advance
      {
        ADVANCE_WAITING,
        ADVANCE_MOUNTED,
        ADVANCE_FAILED
      };

      explicit StandaloneRunControl(const char *applicationName, std::FILE *diagnostics = 0);

      void setApp(App *app);
      Advance advance(bool mainNodeMounted);
      /** Lets a measured Standalone Flow quit only after its terminal audit
          has been published. Ordinary presentation builds keep holding. */
      void observeScenarioAdvance(scenario_tests::ScenarioAdvance advance, const dsl::SnapRecord &record);
      long tick() const;
      bool failed() const;

    private:
      enum
      {
        MOUNT_DEADLINE_TICKS = 5
      };

      App *borrowedApp_;
      const char *applicationName_;
      std::FILE *diagnostics_;
      long tick_;
      bool mountFailed_;
#if LOKA_STANDALONE_PERFORMANCE_RUNS > 0
      StandaloneScenarioVerdict scenarioVerdict_;
#endif
    };
  } // namespace standalone_tests
} // namespace loka

#endif // LOKA_TESTS_STANDALONE_SCENARIO_SUPPORT_HPP
