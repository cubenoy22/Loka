#include "HelloWorldScenarios.hpp"

#include <cassert>

#if !defined(TEST_BUILD)
#error HelloWorld scenarios require TEST_BUILD
#endif

namespace loka
{
  namespace scenario_tests
  {
    namespace
    {
      // One home for HelloWorld's cell names and their order: the reel runs
      // this table front to back, and IsHelloWorldScenario answers from the
      // same array minus the shared startup cell at index 0. Registering a new
      // cell here is what puts it on the reel.
      const char *const kHelloWorldCells[] = {"startup", "toggle-action-probe", "bmi-roundtrip"};
      const std::size_t kHelloWorldCellCount = sizeof(kHelloWorldCells) / sizeof(kHelloWorldCells[0]);
      const char *kToggleActionProbe = kHelloWorldCells[1];
      const char *kBmiRoundtrip = kHelloWorldCells[2];
      const long kInitialTick = 2;
      const long kStepSpacingTicks = 30;

      dsl::FlowChain<app::scene::Scene *, dsl::SnapRecord>
      BuildToggleActionProbeFlow(dsl::testing::ScenarioClock &clock,
                                 app::scene::Scene **sceneInput,
                                 dsl::SnapRecord *recordOut,
                                 dsl::testing::ScenarioAuditSink *audit)
      {
        using namespace dsl::testing;
        const long enabledTick = kInitialTick + kStepSpacingTicks;
        const long disabledTick = enabledTick + kStepSpacingTicks;
        const long finalTick = disabledTick + kStepSpacingTicks;
        return (ScenarioFlow(clock, sceneInput).auditTo(audit)
                | AtTick(kInitialTick,
                         CheckText("HelloWorld.LeftPanel.ActionSummary", "Button enabled: yes / clicks: 0"))
                      .named("verify-initial-action-summary")
                | AtTick(kInitialTick, ClickButton("HelloWorld.LeftPanel.ProbeButton")).named("probe-enabled-action")
                | AtTick(enabledTick,
                         CheckText("HelloWorld.LeftPanel.ActionSummary", "Button enabled: yes / clicks: 1"))
                      .named("verify-enabled-probe")
                | AtTick(enabledTick, ClickButton("HelloWorld.LeftPanel.ToggleEnabledButton"))
                      .named("toggle-probe-disabled")
                | AtTick(disabledTick,
                         CheckText("HelloWorld.LeftPanel.ActionSummary", "Button enabled: no / clicks: 1"))
                      .named("verify-disabled-action-summary")
                | AtTick(disabledTick, ClickButton("HelloWorld.LeftPanel.ProbeButton")).named("probe-disabled-action")
                | AtTick(finalTick, CheckText("HelloWorld.LeftPanel.ActionSummary", "Button enabled: no / clicks: 1"))
                      .named("verify-disabled-probe-was-ignored")
                | AtTick(finalTick,
                         SnapText("HelloWorld.LeftPanel.ActionSummary", "HelloWorld", kToggleActionProbe, finalTick, 1))
                      .named("capture-action-summary")
                      .onSuccess(recordOut))
            .flow();
      }

      dsl::FlowChain<app::scene::Scene *, dsl::SnapRecord>
      BuildBmiRoundtripFlow(dsl::testing::ScenarioClock &clock,
                            app::scene::Scene **sceneInput,
                            dsl::SnapRecord *recordOut,
                            dsl::testing::ScenarioAuditSink *audit)
      {
        using namespace dsl::testing;
        const long calculatedTick = kInitialTick + kStepSpacingTicks;
        const long invalidTick = calculatedTick + kStepSpacingTicks;
        const long finalTick = invalidTick + kStepSpacingTicks;
        return (ScenarioFlow(clock, sceneInput).auditTo(audit)
                | AtTick(kInitialTick, EnterText("HelloWorld.Bmi.HeightInput", "180"))
                      .named("enter-height")
                | AtTick(kInitialTick, EnterText("HelloWorld.Bmi.WeightInput", "81"))
                      .named("enter-weight")
                | AtTick(calculatedTick, CheckText("HelloWorld.Bmi.Result", "BMI: 25.00"))
                      .named("verify-calculated-bmi")
                | AtTick(calculatedTick, EnterText("HelloWorld.Bmi.HeightInput", "invalid"))
                      .named("enter-invalid-height")
                | AtTick(invalidTick, CheckText("HelloWorld.Bmi.Result", "BMI: --"))
                      .named("verify-invalid-input")
                | AtTick(invalidTick, EnterText("HelloWorld.Bmi.HeightInput", "180"))
                      .named("restore-valid-height")
                | AtTick(finalTick, CheckText("HelloWorld.Bmi.Result", "BMI: 25.00"))
                      .named("verify-roundtrip-bmi")
                | AtTick(finalTick,
                         SnapText("HelloWorld.Bmi.Result", "HelloWorld", kBmiRoundtrip, finalTick, 1))
                      .named("capture-bmi-result")
                      .onSuccess(recordOut))
            .flow();
      }
    } // namespace

    ScenarioCellTable HelloWorldReelCells()
    {
      return ScenarioCellTable(kHelloWorldCells, kHelloWorldCellCount);
    }

    bool IsHelloWorldScenario(const std::string &name)
    {
      return HelloWorldReelCells().dropFirst(1).contains(name);
    }

    HelloWorldScenario::ScenarioState::ScenarioState(const std::string &name,
                                                     dsl::testing::ScenarioAuditSink *audit)
        : clock_(),
          scene_(0),
          record_(),
          flow_()
    {
      if (name == kBmiRoundtrip)
      {
        this->flow_.set(BuildBmiRoundtripFlow(this->clock_, &this->scene_, &this->record_, audit));
      }
      else
      {
        this->flow_.set(BuildToggleActionProbeFlow(this->clock_, &this->scene_, &this->record_, audit));
      }
    }

    dsl::FlowRunResult HelloWorldScenario::ScenarioState::run(long tick, app::scene::Scene *scene)
    {
      this->clock_.advanceTo(tick);
      this->scene_ = scene;
      return this->flow_.runResult();
    }

    void HelloWorldScenario::ScenarioState::stop()
    {
      this->flow_.cancel();
      const dsl::FlowRunResult result = this->flow_.runResult();
      assert(result == dsl::FLOW_RUN_CANCELED && "HelloWorld scenario cancellation must be terminal");
      (void)result;
    }

    const dsl::SnapRecord &HelloWorldScenario::ScenarioState::record() const
    {
      return this->record_;
    }

    HelloWorldScenario::HelloWorldScenario(ScenarioCompletionPolicy completionPolicy,
                                           dsl::testing::ScenarioAuditSink *audit)
        : name_(kToggleActionProbe),
          completionPolicy_(completionPolicy),
          terminalState_(SCENARIO_ADVANCE_PENDING),
          terminalAudit_(audit),
          scenarioState_(name_, audit)
    {
    }

    HelloWorldScenario::HelloWorldScenario(const std::string &name,
                                           ScenarioCompletionPolicy completionPolicy,
                                           dsl::testing::ScenarioAuditSink *audit)
        : name_(name),
          completionPolicy_(completionPolicy),
          terminalState_(SCENARIO_ADVANCE_PENDING),
          terminalAudit_(audit),
          scenarioState_(name_, audit)
    {
    }

    ScenarioAdvance HelloWorldScenario::step(long tick,
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
        out = MakeHelloWorldDriverErrorRecord(2401, "scenario expectations were not met");
        out.set("step", this->name_.c_str());
      }
      else
      {
        out = this->scenarioState_.record();
        if (this->name_ == kToggleActionProbe)
        {
          out.set("disabled_probe_ignored", "true");
        }
        else
        {
          out.set("height_cm", "180");
          out.set("weight_kg", "81");
          out.set("invalid_input_result", "BMI: --");
        }
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
          out = MakeHelloWorldDriverErrorRecord(2403, "scenario audit write failed");
        }
        this->terminalState_ = SCENARIO_ADVANCE_FINAL_SCENE_HELD;
        break;
      }
      return this->terminalState_;
    }

    bool HelloWorldScenario::publishVerdict(const dsl::SnapRecord &record)
    {
      std::string verdict;
      const dsl::testing::ScenarioAuditTerminalStatus status =
          record.get("status", verdict) && verdict == dsl::SnapStatusOk()
              ? dsl::testing::SCENARIO_AUDIT_SUCCEEDED
              : dsl::testing::SCENARIO_AUDIT_FAILED;
      return this->terminalAudit_.emit(status, record);
    }

    const std::string &HelloWorldScenario::name() const
    {
      return this->name_;
    }

    void HelloWorldScenario::stop()
    {
      if (this->terminalState_ == SCENARIO_ADVANCE_PENDING)
      {
        this->scenarioState_.stop();
        (void)this->terminalAudit_.emit(dsl::testing::SCENARIO_AUDIT_CANCELED);
        this->terminalState_ = SCENARIO_ADVANCE_FINAL_SCENE_HELD;
      }
    }

    dsl::SnapRecord MakeHelloWorldDriverErrorRecord(long errorCode, const char *message)
    {
      dsl::SnapRecord record;
      record.setInt("format_version", 1);
      record.setInt("schema_version", 1);
      record.setInt("scenario_version", 1);
      record.set("test", "HelloWorld");
      record.set("step", kToggleActionProbe);
      record.set("node", "MainNode");
      record.setInt("tick", 0);
      record.set("status", dsl::SnapStatusError());
      record.setInt("error_code", errorCode);
      record.set("error_msg", message ? message : "driver error");
      return record;
    }
  } // namespace scenario_tests
} // namespace loka
