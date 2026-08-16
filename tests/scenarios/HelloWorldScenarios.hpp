#ifndef LOKA_TESTS_SCENARIOS_HELLO_WORLD_SCENARIOS_HPP
#define LOKA_TESTS_SCENARIOS_HELLO_WORLD_SCENARIOS_HPP

#include <string>

#include "ScenarioTypes.hpp"
#include "app/scene/state/FlowSlot.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace loka
{
  namespace scenario_tests
  {
    /** Returns whether name selects a registered HelloWorld scenario. */
    bool IsHelloWorldScenario(const std::string &name);

    /** Drives HelloWorld through its rendered Buttons so their MainNode-owned
        Emitters remain the meaningful command seam. */
    class HelloWorldScenario
    {
    public:
      explicit HelloWorldScenario(ScenarioCompletionPolicy completionPolicy,
                                  dsl::testing::ScenarioAuditSink *audit = 0);
      HelloWorldScenario(const std::string &name,
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

      HelloWorldScenario(const HelloWorldScenario &);
      HelloWorldScenario &operator=(const HelloWorldScenario &);
    };

    dsl::SnapRecord MakeHelloWorldDriverErrorRecord(long errorCode, const char *message);
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_HELLO_WORLD_SCENARIOS_HPP
