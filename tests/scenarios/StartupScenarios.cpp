#include "StartupScenarios.hpp"

#include "RectSurfaceScenarioObservation.hpp"
#include "app/nodes/controls/Button.hpp"
#include "platform/StringUTF8.hpp"
#include "testing/scene/SceneTestFlow.hpp"

#if !defined(TEST_BUILD)
#error Startup scenarios require TEST_BUILD
#endif

namespace loka
{
  namespace scenario_tests
  {
    namespace
    {
      const char *kStartup = "startup";
      const long kStartupTick = 2;

      const char *TestName(StartupExample example)
      {
        switch (example)
        {
        case STARTUP_EXAMPLE_HELLO_WORLD:
          return "HelloWorld";
        case STARTUP_EXAMPLE_TUTORIAL:
          return "Tutorial";
        case STARTUP_EXAMPLE_MINESWEEPER:
          return "MineSweeper";
        case STARTUP_EXAMPLE_FLOPPY_BIRD:
          return "FloppyBird";
        }
        return "Startup";
      }

      const char *NodeId(StartupExample example)
      {
        switch (example)
        {
        case STARTUP_EXAMPLE_HELLO_WORLD:
          return "HelloWorld.LeftPanel.Title";
        case STARTUP_EXAMPLE_TUTORIAL:
          return "Tutorial.Startup.Title";
        case STARTUP_EXAMPLE_MINESWEEPER:
          return "MineSweeper.NewGameButton";
        case STARTUP_EXAMPLE_FLOPPY_BIRD:
          return "FloppyBird.Surface";
        }
        return "Startup";
      }

      const char *ExpectedText(StartupExample example)
      {
        switch (example)
        {
        case STARTUP_EXAMPLE_HELLO_WORLD:
          return "Loka Sample";
        case STARTUP_EXAMPLE_TUTORIAL:
          return "Loka Tutorial";
        case STARTUP_EXAMPLE_MINESWEEPER:
          return "New Game";
        case STARTUP_EXAMPLE_FLOPPY_BIRD:
          return "";
        }
        return "";
      }

      dsl::StepRunStatus BuildTextRecord(StartupExample example,
                                         app::scene::Scene *scene,
                                         dsl::SnapRecord &out,
                                         dsl::FlowError &error)
      {
        const dsl::StepRunStatus captured =
            dsl::testing::SnapText(NodeId(example), TestName(example), kStartup, kStartupTick, 1)
                .run(scene, out, error);
        if (captured != dsl::FLOW_STEP_SUCCEEDED)
        {
          return captured;
        }
        std::string actual;
        if (!out.get("text.value", actual) || actual != ExpectedText(example))
        {
          error.kind = dsl::testing::FLOW_ERROR_KIND_SCENE_TEST_ASSERT;
          error.code = dsl::testing::FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED;
          return dsl::FLOW_STEP_FAILED;
        }
        return dsl::FLOW_STEP_SUCCEEDED;
      }

      dsl::StepRunStatus BuildButtonRecord(StartupExample example,
                                           app::scene::Scene *scene,
                                           dsl::SnapRecord &out,
                                           dsl::FlowError &error)
      {
        app::ButtonNode *button = 0;
        const dsl::StepRunStatus lookup =
            dsl::testing::LookupNodeById<app::ButtonNode>(scene, NodeId(example), button, error);
        if (lookup != dsl::FLOW_STEP_SUCCEEDED)
        {
          return lookup;
        }
        std::string text;
        if (!button || !button->props.text_ || !platform::CollectUtf8(button->props.text_->get(), text)
            || text != ExpectedText(example))
        {
          error.kind = dsl::testing::FLOW_ERROR_KIND_SCENE_TEST_ASSERT;
          error.code = dsl::testing::FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED;
          return dsl::FLOW_STEP_FAILED;
        }
        dsl::BuildSnapV1RecordAdapter base(
            TestName(example), kStartup, NodeId(example), kStartupTick, 1, dsl::SnapStatusOk());
        const int unused = 0;
        dsl::FlowError ignored;
        if (base.run(unused, out, ignored) != dsl::FLOW_STEP_SUCCEEDED)
        {
          error.kind = dsl::FLOW_ERROR_KIND_SNAP;
          error.code = dsl::FLOW_ERROR_SNAP_WRITE_FAILED;
          return dsl::FLOW_STEP_FAILED;
        }
        out.set("button.text", text.c_str());
        return dsl::FLOW_STEP_SUCCEEDED;
      }

      dsl::StepRunStatus BuildSurfaceRecord(StartupExample example,
                                            app::scene::Scene *scene,
                                            dsl::SnapRecord &out,
                                            dsl::FlowError &error)
      {
        std::string rectangles;
        const dsl::StepRunStatus capture =
            CaptureRectSurfaceModel(scene, NodeId(example), rectangles, error);
        if (capture != dsl::FLOW_STEP_SUCCEEDED)
        {
          return capture;
        }
        if (rectangles != "72,114,18,14")
        {
          error.kind = dsl::testing::FLOW_ERROR_KIND_SCENE_TEST_ASSERT;
          error.code = dsl::testing::FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED;
          return dsl::FLOW_STEP_FAILED;
        }
        dsl::BuildSnapV1RecordAdapter base(
            TestName(example), kStartup, NodeId(example), kStartupTick, 1, dsl::SnapStatusOk());
        const int unused = 0;
        dsl::FlowError ignored;
        if (base.run(unused, out, ignored) != dsl::FLOW_STEP_SUCCEEDED)
        {
          error.kind = dsl::FLOW_ERROR_KIND_SNAP;
          error.code = dsl::FLOW_ERROR_SNAP_WRITE_FAILED;
          return dsl::FLOW_STEP_FAILED;
        }
        out.set("surface.rects", rectangles.c_str());
        return dsl::FLOW_STEP_SUCCEEDED;
      }

      dsl::StepRunStatus BuildStartupRecord(StartupExample example,
                                            app::scene::Scene *scene,
                                            dsl::SnapRecord &out,
                                            dsl::FlowError &error)
      {
        switch (example)
        {
        case STARTUP_EXAMPLE_HELLO_WORLD:
        case STARTUP_EXAMPLE_TUTORIAL:
          return BuildTextRecord(example, scene, out, error);
        case STARTUP_EXAMPLE_MINESWEEPER:
          return BuildButtonRecord(example, scene, out, error);
        case STARTUP_EXAMPLE_FLOPPY_BIRD:
          return BuildSurfaceRecord(example, scene, out, error);
        }
        return dsl::FLOW_STEP_FAILED;
      }
    } // namespace

    bool IsStartupScenario(const std::string &name)
    {
      return name == kStartup;
    }

    StartupScenario::StartupScenario(StartupExample example,
                                     ScenarioCompletionPolicy completionPolicy,
                                     dsl::testing::ScenarioAuditSink *audit)
        : example_(example),
          name_(kStartup),
          completionPolicy_(completionPolicy),
          terminalState_(SCENARIO_ADVANCE_PENDING),
          terminalAudit_(audit)
    {
    }

    ScenarioAdvance StartupScenario::step(long tick,
                                           app::scene::Scene *scene,
                                           const CaptureContentBounds &bounds,
                                           dsl::SnapRecord &out)
    {
      if (this->terminalState_ != SCENARIO_ADVANCE_PENDING)
      {
        return this->terminalState_;
      }
      if (tick < kStartupTick)
      {
        return SCENARIO_ADVANCE_PENDING;
      }
      dsl::FlowError error;
      if (BuildStartupRecord(this->example_, scene, out, error) != dsl::FLOW_STEP_SUCCEEDED)
      {
        out = MakeStartupDriverErrorRecord(this->example_, 2801, "startup expectations were not met");
      }
      else
      {
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
          out = MakeStartupDriverErrorRecord(this->example_, 2803, "startup audit write failed");
        }
        this->terminalState_ = SCENARIO_ADVANCE_FINAL_SCENE_HELD;
        break;
      }
      return this->terminalState_;
    }

    bool StartupScenario::publishVerdict(const dsl::SnapRecord &record)
    {
      std::string verdict;
      const dsl::testing::ScenarioAuditTerminalStatus status =
          record.get("status", verdict) && verdict == dsl::SnapStatusOk()
              ? dsl::testing::SCENARIO_AUDIT_SUCCEEDED
              : dsl::testing::SCENARIO_AUDIT_FAILED;
      return this->terminalAudit_.emit(status, record);
    }

    const std::string &StartupScenario::name() const
    {
      return this->name_;
    }

    void StartupScenario::stop()
    {
      if (this->terminalState_ == SCENARIO_ADVANCE_PENDING)
      {
        (void)this->terminalAudit_.emit(dsl::testing::SCENARIO_AUDIT_CANCELED);
        this->terminalState_ = SCENARIO_ADVANCE_FINAL_SCENE_HELD;
      }
    }

    dsl::SnapRecord MakeStartupDriverErrorRecord(StartupExample example,
                                                 long errorCode,
                                                 const char *message)
    {
      dsl::SnapRecord record;
      record.setInt("format_version", 1);
      record.setInt("schema_version", 1);
      record.setInt("scenario_version", 1);
      record.set("test", TestName(example));
      record.set("step", kStartup);
      record.set("node", NodeId(example));
      record.setInt("tick", 0);
      record.set("status", dsl::SnapStatusError());
      record.setInt("error_code", errorCode);
      record.set("error_msg", message ? message : "startup driver error");
      return record;
    }
  } // namespace scenario_tests
} // namespace loka
