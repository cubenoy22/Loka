#ifndef LOKA_TESTS_SCENARIOS_STARTUP_SCENARIOS_HPP
#define LOKA_TESTS_SCENARIOS_STARTUP_SCENARIOS_HPP

#include <string>

#include "ScenarioTypes.hpp"
#include "testing/scene/ScenarioAudit.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class Scene;
    }
  } // namespace app

  namespace scenario_tests
  {
    enum StartupExample
    {
      STARTUP_EXAMPLE_HELLO_WORLD = 0,
      STARTUP_EXAMPLE_TUTORIAL,
      STARTUP_EXAMPLE_MINESWEEPER,
      STARTUP_EXAMPLE_FLOPPY_BIRD
    };

    /** Returns whether name selects the shared settled initial-screen scenario. */
    bool IsStartupScenario(const std::string &name);

    /** Observes one example's initial screen at a fixed tick and owns its
        terminal audit state. */
    class StartupScenario
    {
    public:
      StartupScenario(StartupExample example,
                      ScenarioCompletionPolicy completionPolicy,
                      dsl::testing::ScenarioAuditSink *audit = 0);

      ScenarioAdvance
      step(long tick, app::scene::Scene *scene, const CaptureContentBounds &bounds, dsl::SnapRecord &out);
      bool publishVerdict(const dsl::SnapRecord &record);
      const std::string &name() const;
      void stop();

    private:
      const StartupExample example_;
      const std::string name_;
      const ScenarioCompletionPolicy completionPolicy_;
      ScenarioAdvance terminalState_;
      dsl::testing::scenario_audit_detail::TerminalEmitter terminalAudit_;

      StartupScenario(const StartupScenario &);
      StartupScenario &operator=(const StartupScenario &);
    };

    dsl::SnapRecord MakeStartupDriverErrorRecord(StartupExample example,
                                                 long errorCode,
                                                 const char *message);
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_STARTUP_SCENARIOS_HPP
