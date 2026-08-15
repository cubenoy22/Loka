#ifndef LOKA_TESTS_SCENARIOS_TUTORIAL_SCENARIOS_HPP
#define LOKA_TESTS_SCENARIOS_TUTORIAL_SCENARIOS_HPP

#include <string>

#include "ScenarioTypes.hpp"
#include "app/scene/state/FlowSlot.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace loka
{
  namespace scenario_tests
  {
    /** Returns whether name selects Tutorial's increment/summary scenario. */
    bool IsTutorialScenario(const std::string &name);

    /** Drives Tutorial Step 4 through its rendered controls and observes the
        resulting projection through TEST_ID queries. */
    class TutorialScenario
    {
    public:
      explicit TutorialScenario(ScenarioCompletionPolicy completionPolicy,
                                dsl::testing::ScenarioAuditSink *audit = 0);

      ScenarioAdvance
      step(long tick, app::scene::Scene *scene, const CaptureContentBounds &bounds, dsl::SnapRecord &out);
      /** Publishes the completed driver-owned verdict after rail-local checks. */
      bool publishVerdict(const dsl::SnapRecord &record);
      const std::string &name() const;
      void stop();

    private:
      typedef dsl::FlowChain<app::scene::Scene *, dsl::SnapRecord> IncrementSummaryToggleFlowChain;

      class IncrementSummaryToggleState
      {
      public:
        explicit IncrementSummaryToggleState(dsl::testing::ScenarioAuditSink *audit);

        dsl::FlowRunResult run(long tick, app::scene::Scene *scene);
        void stop();
        const dsl::SnapRecord &record() const;

      private:
        dsl::testing::ScenarioClock clock_;
        app::scene::Scene *scene_;
        dsl::SnapRecord record_;
        app::scene::FlowSlot<IncrementSummaryToggleFlowChain> flow_;

        IncrementSummaryToggleState(const IncrementSummaryToggleState &);
        IncrementSummaryToggleState &operator=(const IncrementSummaryToggleState &);
      };

      const std::string name_;
      const ScenarioCompletionPolicy completionPolicy_;
      ScenarioAdvance terminalState_;
      dsl::testing::scenario_audit_detail::TerminalEmitter terminalAudit_;
      IncrementSummaryToggleState scenarioState_;

      TutorialScenario(const TutorialScenario &);
      TutorialScenario &operator=(const TutorialScenario &);
    };

    dsl::SnapRecord MakeTutorialDriverErrorRecord(long errorCode, const char *message);
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_TUTORIAL_SCENARIOS_HPP
