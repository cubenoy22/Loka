#include "TutorialScenarios.hpp"

#include <cassert>

#if !defined(TEST_BUILD)
#error Tutorial scenarios require TEST_BUILD
#endif

namespace loka
{
  namespace scenario_tests
  {
    namespace
    {
      const char *kIncrementSummaryToggle = "increment-summary-toggle";
      const char *kSummary = "Tutorial.Step4.Summary";
      const char *kAddItemButton = "Tutorial.Step4.AddItemButton";
      const char *kToggleSummaryButton = "Tutorial.Step4.ToggleSummaryButton";
      const long kInitialTick = 2;
      const long kStepSpacingTicks = 30;

      class CheckNodeAbsentAdapter
      {
      public:
        typedef app::scene::Scene *In;
        typedef app::scene::Scene *Out;

        explicit CheckNodeAbsentAdapter(const char *testId)
            : testId_(testId ? testId : "")
        {
        }

        dsl::StepRunStatus run(In const &in, Out &out, dsl::FlowError &error) const
        {
          out = in;
          app::scene::Node *node = 0;
          dsl::FlowError lookupError;
          const dsl::StepRunStatus status =
              dsl::testing::LookupNodeById<app::scene::Node>(in, this->testId_, node, lookupError);
          if (status == dsl::FLOW_STEP_FAILED
              && lookupError.kind == dsl::testing::FLOW_ERROR_KIND_SCENE_SCENARIO
              && lookupError.code == dsl::testing::FLOW_ERROR_SCENE_TEST_NODE_NOT_FOUND)
          {
            return dsl::FLOW_STEP_SUCCEEDED;
          }
          if (status == dsl::FLOW_STEP_SUCCEEDED)
          {
            error.kind = dsl::testing::FLOW_ERROR_KIND_SCENE_TEST_ASSERT;
            error.code = dsl::testing::FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED;
            return dsl::FLOW_STEP_FAILED;
          }
          error = lookupError;
          return status;
        }

      private:
        std::string testId_;
      };

      CheckNodeAbsentAdapter CheckNodeAbsent(const char *testId)
      {
        return CheckNodeAbsentAdapter(testId);
      }

      dsl::FlowChain<app::scene::Scene *, dsl::SnapRecord>
      BuildIncrementSummaryToggleFlow(dsl::testing::ScenarioClock &clock,
                                      app::scene::Scene **sceneInput,
                                      dsl::SnapRecord *recordOut,
                                      dsl::testing::ScenarioAuditSink *audit)
      {
        using namespace dsl::testing;
        const long firstIncrementTick = kInitialTick + kStepSpacingTicks;
        const long secondIncrementTick = firstIncrementTick + kStepSpacingTicks;
        const long hiddenTick = secondIncrementTick + kStepSpacingTicks;
        const long finalTick = hiddenTick + kStepSpacingTicks;
        return (ScenarioFlow(clock, sceneInput).auditTo(audit)
                | AtTick(kInitialTick, CheckText(kSummary, "Items: 0")).named("verify-initial-summary")
                | AtTick(kInitialTick, ClickButton(kAddItemButton)).named("increment-to-one")
                | AtTick(firstIncrementTick, CheckText(kSummary, "Items: 1")).named("verify-first-increment")
                | AtTick(firstIncrementTick, ClickButton(kAddItemButton)).named("increment-to-two")
                | AtTick(secondIncrementTick, CheckText(kSummary, "Items: 2")).named("verify-second-increment")
                | AtTick(secondIncrementTick, ClickButton(kToggleSummaryButton)).named("hide-summary")
                | AtTick(hiddenTick, CheckNodeAbsent(kSummary)).named("verify-summary-hidden")
                | AtTick(hiddenTick, ClickButton(kToggleSummaryButton)).named("show-summary")
                | AtTick(finalTick, CheckText(kSummary, "Items: 2")).named("verify-restored-summary")
                | AtTick(finalTick, SnapText(kSummary, "Tutorial", kIncrementSummaryToggle, finalTick, 1))
                      .named("capture-restored-summary")
                      .onSuccess(recordOut))
            .flow();
      }
    } // namespace

    bool IsTutorialScenario(const std::string &name)
    {
      return name == kIncrementSummaryToggle;
    }

    TutorialScenario::IncrementSummaryToggleState::IncrementSummaryToggleState(
        dsl::testing::ScenarioAuditSink *audit)
        : clock_(),
          scene_(0),
          record_(),
          flow_()
    {
      this->flow_.set(BuildIncrementSummaryToggleFlow(this->clock_, &this->scene_, &this->record_, audit));
    }

    dsl::FlowRunResult TutorialScenario::IncrementSummaryToggleState::run(long tick, app::scene::Scene *scene)
    {
      this->clock_.advanceTo(tick);
      this->scene_ = scene;
      return this->flow_.runResult();
    }

    void TutorialScenario::IncrementSummaryToggleState::stop()
    {
      this->flow_.cancel();
      const dsl::FlowRunResult result = this->flow_.runResult();
      assert(result == dsl::FLOW_RUN_CANCELED && "Tutorial scenario cancellation must be terminal");
      (void)result;
    }

    const dsl::SnapRecord &TutorialScenario::IncrementSummaryToggleState::record() const
    {
      return this->record_;
    }

    TutorialScenario::TutorialScenario(ScenarioCompletionPolicy completionPolicy,
                                       dsl::testing::ScenarioAuditSink *audit)
        : name_(kIncrementSummaryToggle),
          completionPolicy_(completionPolicy),
          terminalState_(SCENARIO_ADVANCE_PENDING),
          terminalAudit_(audit),
          scenarioState_(audit)
    {
    }

    ScenarioAdvance TutorialScenario::step(long tick,
                                            app::scene::Scene *scene,
                                            const CaptureContentBounds &bounds,
                                            dsl::SnapRecord &out)
    {
      if (this->terminalState_ != SCENARIO_ADVANCE_PENDING)
      {
        return this->terminalState_;
      }
      const dsl::FlowRunResult result = this->scenarioState_.run(tick, scene);
      if (result == dsl::FLOW_RUN_PENDING)
      {
        return SCENARIO_ADVANCE_PENDING;
      }
      if (result != dsl::FLOW_RUN_SUCCEEDED)
      {
        out = MakeTutorialDriverErrorRecord(2501, "scenario expectations were not met");
      }
      else
      {
        out = this->scenarioState_.record();
        out.setInt("increment_count", 2);
        out.set("summary_toggle_round_trip", "true");
        SetContentBounds(out, bounds);
      }
      switch (this->completionPolicy_)
      {
      case SCENARIO_COMPLETION_DRIVER_OWNED:
        this->terminalState_ = SCENARIO_ADVANCE_DRIVER_COMPLETION_READY;
        break;
      case SCENARIO_COMPLETION_HOLD_FINAL_SCENE:
        if (!this->publishVerdict(out))
        {
          out = MakeTutorialDriverErrorRecord(2503, "scenario audit write failed");
        }
        this->terminalState_ = SCENARIO_ADVANCE_FINAL_SCENE_HELD;
        break;
      }
      return this->terminalState_;
    }

    bool TutorialScenario::publishVerdict(const dsl::SnapRecord &record)
    {
      std::string verdict;
      const dsl::testing::ScenarioAuditTerminalStatus status =
          record.get("status", verdict) && verdict == dsl::SnapStatusOk()
              ? dsl::testing::SCENARIO_AUDIT_SUCCEEDED
              : dsl::testing::SCENARIO_AUDIT_FAILED;
      return this->terminalAudit_.emit(status, record);
    }

    const std::string &TutorialScenario::name() const
    {
      return this->name_;
    }

    void TutorialScenario::stop()
    {
      if (this->terminalState_ == SCENARIO_ADVANCE_PENDING)
      {
        this->scenarioState_.stop();
        (void)this->terminalAudit_.emit(dsl::testing::SCENARIO_AUDIT_CANCELED);
        this->terminalState_ = SCENARIO_ADVANCE_FINAL_SCENE_HELD;
      }
    }

    dsl::SnapRecord MakeTutorialDriverErrorRecord(long errorCode, const char *message)
    {
      dsl::SnapRecord record;
      record.setInt("format_version", 1);
      record.setInt("schema_version", 1);
      record.setInt("scenario_version", 1);
      record.set("test", "Tutorial");
      record.set("step", kIncrementSummaryToggle);
      record.set("node", "Tutorial.Step4");
      record.setInt("tick", 0);
      record.set("status", dsl::SnapStatusError());
      record.setInt("error_code", errorCode);
      record.set("error_msg", message ? message : "driver error");
      return record;
    }
  } // namespace scenario_tests
} // namespace loka
