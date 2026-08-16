#ifndef LOKA_TESTS_SCENARIOS_MINESWEEPER_SCENARIOS_HPP
#define LOKA_TESTS_SCENARIOS_MINESWEEPER_SCENARIOS_HPP

#include <string>

#include "ScenarioTypes.hpp"
#include "app/scene/state/FlowSlot.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace loka
{
  namespace scenario_tests
  {
    /** The caller-owned input shared by MineSweeper's deterministic scenario
        rail and standalone presentation. */
    unsigned long MineSweeperScenarioSeed();

    /** Returns whether name selects a registered MineSweeper scenario. */
    bool IsMineSweeperScenario(const std::string &name);

    /** Drives New Game twice through MainNode's rendered Button and observes
        each completed board through its immutable MineCell props. */
    class MineSweeperScenario
    {
    public:
      explicit MineSweeperScenario(ScenarioCompletionPolicy completionPolicy,
                                   dsl::testing::ScenarioAuditSink *audit = 0);
      MineSweeperScenario(const std::string &name,
                          ScenarioCompletionPolicy completionPolicy,
                          dsl::testing::ScenarioAuditSink *audit = 0);

      ScenarioAdvance
      step(long tick, app::scene::Scene *scene, const CaptureContentBounds &bounds, dsl::SnapRecord &out);
      /** Publishes the completed driver-owned verdict after rail-local checks. */
      bool publishVerdict(const dsl::SnapRecord &record);
      const std::string &name() const;
      void stop();

    private:
      typedef dsl::FlowChain<app::scene::Scene *, dsl::SnapRecord> ScenarioFlowChain;

      class ScenarioState
      {
      public:
        ScenarioState(const std::string &name, dsl::testing::ScenarioAuditSink *audit);

        dsl::FlowRunResult run(long tick, app::scene::Scene *scene);
        void stop();
        const dsl::SnapRecord &record() const;

      private:
        dsl::testing::ScenarioClock clock_;
        app::scene::Scene *scene_;
        dsl::SnapRecord record_;
        app::scene::FlowSlot<ScenarioFlowChain> flow_;

        ScenarioState(const ScenarioState &);
        ScenarioState &operator=(const ScenarioState &);
      };

      const std::string name_;
      const ScenarioCompletionPolicy completionPolicy_;
      ScenarioAdvance terminalState_;
      dsl::testing::scenario_audit_detail::TerminalEmitter terminalAudit_;
      ScenarioState scenarioState_;

      MineSweeperScenario(const MineSweeperScenario &);
      MineSweeperScenario &operator=(const MineSweeperScenario &);
    };

    dsl::SnapRecord MakeMineSweeperDriverErrorRecord(long errorCode, const char *message);
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_MINESWEEPER_SCENARIOS_HPP
