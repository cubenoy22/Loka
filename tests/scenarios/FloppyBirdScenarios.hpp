#ifndef LOKA_TESTS_SCENARIOS_FLOPPY_BIRD_SCENARIOS_HPP
#define LOKA_TESTS_SCENARIOS_FLOPPY_BIRD_SCENARIOS_HPP

#include <string>

#include "../../example/FloppyBird/src/GameModel.hpp"
#include "ScenarioTypes.hpp"
#include "app/scene/state/FlowSlot.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace loka
{
  namespace scenario_tests
  {
    /** Returns the fixed caller-owned seed used by the FloppyBird scenario. */
    unsigned long FloppyBirdScenarioSeed();

    /** Returns whether name selects FloppyBird's fixed-step flap scenario. */
    bool IsFloppyBirdScenario(const std::string &name);

    /** Drives one seeded FloppyBird run with flaps and observations scheduled
        only by fixed simulation steps. */
    class FloppyBirdScenario
    {
    public:
      explicit FloppyBirdScenario(ScenarioCompletionPolicy completionPolicy,
                                  dsl::testing::ScenarioAuditSink *audit = 0);

      ScenarioAdvance step(long tick,
                           app::scene::Scene *scene,
                           floppybird::GameModel &game,
                           const CaptureContentBounds &bounds,
                           dsl::SnapRecord &out);
      /** Publishes the completed driver-owned verdict after rail-local checks. */
      bool publishVerdict(const dsl::SnapRecord &record);
      const std::string &name() const;
      void stop();

      /** One tick's borrowed scene and game owner. Public only so typed Flow
          adapters can name the value; scenario callers construct none. */
      struct ScenarioInput
      {
        ScenarioInput()
            : scene(0),
              game(0)
        {
        }

        app::scene::Scene *scene;
        floppybird::GameModel *game;
      };

    private:

      typedef dsl::FlowChain<ScenarioInput, dsl::SnapRecord> FixedStepFlapFlowChain;

      class FixedStepFlapState
      {
      public:
        explicit FixedStepFlapState(dsl::testing::ScenarioAuditSink *audit);

        dsl::FlowRunResult run(long tick, app::scene::Scene *scene, floppybird::GameModel &game);
        void stop();
        const dsl::SnapRecord &record() const;

      private:
        dsl::testing::ScenarioClock clock_;
        ScenarioInput input_;
        dsl::SnapRecord record_;
        app::scene::FlowSlot<FixedStepFlapFlowChain> flow_;

        FixedStepFlapState(const FixedStepFlapState &);
        FixedStepFlapState &operator=(const FixedStepFlapState &);
      };

      const std::string name_;
      const ScenarioCompletionPolicy completionPolicy_;
      ScenarioAdvance terminalState_;
      dsl::testing::scenario_audit_detail::TerminalEmitter terminalAudit_;
      FixedStepFlapState scenarioState_;

      FloppyBirdScenario(const FloppyBirdScenario &);
      FloppyBirdScenario &operator=(const FloppyBirdScenario &);
    };

    dsl::SnapRecord MakeFloppyBirdDriverErrorRecord(long errorCode, const char *message);
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_FLOPPY_BIRD_SCENARIOS_HPP
