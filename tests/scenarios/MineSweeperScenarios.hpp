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

    /** Returns whether name selects MineSweeper's two-New-Game scenario. */
    bool IsMineSweeperScenario(const std::string &name);

    /** Drives New Game twice through MainNode's rendered Button and observes
        each completed board through its immutable MineCell props. */
    class MineSweeperScenario
    {
    public:
      explicit MineSweeperScenario(ScenarioCompletionPolicy completionPolicy,
                                   dsl::testing::ScenarioAuditSink *audit = 0);

      ScenarioAdvance
      step(long tick, app::scene::Scene *scene, const CaptureContentBounds &bounds, dsl::SnapRecord &out);
      /** Publishes the completed driver-owned verdict after rail-local checks. */
      bool publishVerdict(const dsl::SnapRecord &record);
      const std::string &name() const;
      void stop();

    private:
      typedef dsl::FlowChain<app::scene::Scene *, dsl::SnapRecord> NewGameTwiceFlowChain;

      class NewGameTwiceState
      {
      public:
        explicit NewGameTwiceState(dsl::testing::ScenarioAuditSink *audit);

        dsl::FlowRunResult run(long tick, app::scene::Scene *scene);
        void stop();
        const dsl::SnapRecord &record() const;

      private:
        dsl::testing::ScenarioClock clock_;
        app::scene::Scene *scene_;
        dsl::SnapRecord record_;
        app::scene::FlowSlot<NewGameTwiceFlowChain> flow_;

        NewGameTwiceState(const NewGameTwiceState &);
        NewGameTwiceState &operator=(const NewGameTwiceState &);
      };

      const std::string name_;
      const ScenarioCompletionPolicy completionPolicy_;
      ScenarioAdvance terminalState_;
      dsl::testing::scenario_audit_detail::TerminalEmitter terminalAudit_;
      NewGameTwiceState scenarioState_;

      MineSweeperScenario(const MineSweeperScenario &);
      MineSweeperScenario &operator=(const MineSweeperScenario &);
    };

    dsl::SnapRecord MakeMineSweeperDriverErrorRecord(long errorCode, const char *message);
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_MINESWEEPER_SCENARIOS_HPP
