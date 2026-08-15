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
      const char *kToggleActionProbe = "toggle-action-probe";
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
    } // namespace

    bool IsHelloWorldScenario(const std::string &name)
    {
      return name == kToggleActionProbe;
    }

    HelloWorldScenario::ToggleActionProbeState::ToggleActionProbeState(dsl::testing::ScenarioAuditSink *audit)
        : clock_(),
          scene_(0),
          record_(),
          terminalAudit_(audit),
          flow_()
    {
      this->flow_.set(BuildToggleActionProbeFlow(this->clock_, &this->scene_, &this->record_, audit));
    }

    dsl::FlowRunResult HelloWorldScenario::ToggleActionProbeState::run(long tick, app::scene::Scene *scene)
    {
      this->clock_.advanceTo(tick);
      this->scene_ = scene;
      return this->flow_.runResult();
    }

    bool HelloWorldScenario::ToggleActionProbeState::finish(dsl::testing::ScenarioAuditTerminalStatus status)
    {
      return this->terminalAudit_.emit(status);
    }

    void HelloWorldScenario::ToggleActionProbeState::stop()
    {
      if (this->terminalAudit_.isSettled())
      {
        return;
      }
      this->flow_.cancel();
      const dsl::FlowRunResult result = this->flow_.runResult();
      assert(result == dsl::FLOW_RUN_CANCELED && "HelloWorld scenario cancellation must be terminal");
      (void)result;
      (void)this->terminalAudit_.emit(dsl::testing::SCENARIO_AUDIT_CANCELED);
    }

    const dsl::SnapRecord &HelloWorldScenario::ToggleActionProbeState::record() const
    {
      return this->record_;
    }

    HelloWorldScenario::HelloWorldScenario(ScenarioCompletionPolicy completionPolicy,
                                           dsl::testing::ScenarioAuditSink *audit)
        : name_(kToggleActionProbe),
          completionPolicy_(completionPolicy),
          terminalState_(SCENARIO_ADVANCE_PENDING),
          probe_(audit)
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
      const dsl::FlowRunResult result = this->probe_.run(tick, scene);
      if (result == dsl::FLOW_RUN_PENDING)
      {
        return SCENARIO_ADVANCE_PENDING;
      }
      if (result != dsl::FLOW_RUN_SUCCEEDED)
      {
        (void)this->probe_.finish(dsl::testing::SCENARIO_AUDIT_FAILED);
        out = MakeHelloWorldDriverErrorRecord(2401, "scenario expectations were not met");
      }
      else
      {
        (void)this->probe_.finish(dsl::testing::SCENARIO_AUDIT_SUCCEEDED);
        out = this->probe_.record();
        out.set("disabled_probe_ignored", "true");
        SetContentBounds(out, bounds);
      }
      switch (this->completionPolicy_)
      {
      case SCENARIO_COMPLETION_DRIVER_OWNED:
        this->terminalState_ = SCENARIO_ADVANCE_DRIVER_COMPLETION_READY;
        break;
      case SCENARIO_COMPLETION_HOLD_FINAL_SCENE:
        this->terminalState_ = SCENARIO_ADVANCE_FINAL_SCENE_HELD;
        break;
      }
      return this->terminalState_;
    }

    const std::string &HelloWorldScenario::name() const
    {
      return this->name_;
    }

    void HelloWorldScenario::stop()
    {
      if (this->terminalState_ == SCENARIO_ADVANCE_PENDING)
      {
        this->probe_.stop();
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
