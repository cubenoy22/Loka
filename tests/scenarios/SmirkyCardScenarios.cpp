#include "SmirkyCardScenarios.hpp"

#include "app/nodes/controls/Button.hpp"
#include "testing/scene/SceneTestFlow.hpp"

#if !defined(TEST_BUILD)
#error SmirkyCard scenarios require TEST_BUILD
#endif

namespace loka
{
  namespace scenario_tests
  {
    namespace
    {
      const char *const kCells[] = {"startup", "run-javascript"};
      const std::size_t kCellCount = sizeof(kCells) / sizeof(kCells[0]);

      bool CaptureTitle(app::scene::Scene *scene, const char *step, long tick, dsl::SnapRecord &out)
      {
        dsl::FlowError error;
        return dsl::testing::SnapText("SmirkyCard.Title", "SmirkyCard", step, tick, 1).run(scene, out, error)
            == dsl::FLOW_STEP_SUCCEEDED;
      }
    }

    ScenarioCellTable SmirkyCardReelCells() { return ScenarioCellTable(kCells, kCellCount); }
    bool IsSmirkyCardScenario(const std::string &name) { return SmirkyCardReelCells().contains(name); }

    SmirkyCardScenario::SmirkyCardScenario(const std::string &name, ScenarioCompletionPolicy completionPolicy,
                                           dsl::testing::ScenarioAuditSink *audit)
        : name_(name), completionPolicy_(completionPolicy), terminalState_(SCENARIO_ADVANCE_PENDING),
          emitted_(false), terminalAudit_(audit) {}

    ScenarioAdvance SmirkyCardScenario::step(long tick, app::scene::Scene *scene,
                                              const CaptureContentBounds &bounds, dsl::SnapRecord &out)
    {
      if (this->terminalState_ != SCENARIO_ADVANCE_PENDING) return this->terminalState_;
      if (!scene || tick < 2) return SCENARIO_ADVANCE_PENDING;
      if (this->name_ == "run-javascript" && !this->emitted_)
      {
        app::ButtonNode *button = 0;
        dsl::FlowError error;
        if (dsl::testing::LookupNodeById<app::ButtonNode>(scene, "SmirkyCard.Run", button, error)
                != dsl::FLOW_STEP_SUCCEEDED || !button || !button->props.getOnClick())
        {
          out = MakeSmirkyCardDriverErrorRecord(2901, "Run JavaScript button was not mounted");
          this->terminalState_ = SCENARIO_ADVANCE_DRIVER_COMPLETION_READY;
          return this->terminalState_;
        }
        button->props.getOnClick()->emit();
        this->emitted_ = true;
        return SCENARIO_ADVANCE_PENDING;
      }
      const char *expected = this->name_ == "run-javascript" ? "Card Two" : "Card One";
      if (!CaptureTitle(scene, this->name_.c_str(), tick, out))
        out = MakeSmirkyCardDriverErrorRecord(2902, "Card title was not mounted");
      else
      {
        std::string title;
        if (!out.get("text.value", title) || title != expected)
          out = MakeSmirkyCardDriverErrorRecord(2903, "admitted Card title did not match expected Scene");
        else
          SetContentBounds(out, bounds);
      }
      if (this->completionPolicy_ == SCENARIO_COMPLETION_HOLD_FINAL_SCENE)
      {
        (void)this->publishVerdict(out);
        this->terminalState_ = SCENARIO_ADVANCE_FINAL_SCENE_HELD;
      }
      else this->terminalState_ = SCENARIO_ADVANCE_DRIVER_COMPLETION_READY;
      return this->terminalState_;
    }

    bool SmirkyCardScenario::publishVerdict(const dsl::SnapRecord &record)
    {
      std::string status;
      return this->terminalAudit_.emit(record.get("status", status) && status == dsl::SnapStatusOk()
                                           ? dsl::testing::SCENARIO_AUDIT_SUCCEEDED
                                           : dsl::testing::SCENARIO_AUDIT_FAILED, record);
    }
    void SmirkyCardScenario::stop()
    {
      if (this->terminalState_ == SCENARIO_ADVANCE_PENDING)
      {
        (void)this->terminalAudit_.emit(dsl::testing::SCENARIO_AUDIT_CANCELED);
        this->terminalState_ = SCENARIO_ADVANCE_FINAL_SCENE_HELD;
      }
    }
    dsl::SnapRecord MakeSmirkyCardDriverErrorRecord(long code, const char *message)
    {
      dsl::SnapRecord record;
      record.setInt("format_version", 1); record.setInt("schema_version", 1); record.setInt("scenario_version", 1);
      record.set("test", "SmirkyCard"); record.set("step", "run-javascript"); record.set("node", "SmirkyCard.Title");
      record.setInt("tick", 0); record.set("status", dsl::SnapStatusError()); record.setInt("error_code", code);
      record.set("error_msg", message ? message : "driver error"); return record;
    }
  }
}
