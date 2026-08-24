#include "FloppyBirdScenarios.hpp"

#include <cassert>
#include "RectSurfaceScenarioObservation.hpp"

#if !defined(TEST_BUILD)
#error FloppyBird scenarios require TEST_BUILD
#endif

namespace loka
{
  namespace scenario_tests
  {
    namespace
    {
      const char *kFixedStepFlaps = "fixed-step-flaps";
      const char *kSurface = "FloppyBird.Surface";
      const char *kWaitingSurface = "72,114,18,14";
      const char *kStartedSurface = "72,113,18,14";
      const char *kFirstPipeSurface = "318,0,24,60;318,132,24,108;72,97,18,14";
      const char *kMidRunSurface =
          "83,0,24,60;83,132,24,108;256,0,24,39;256,111,24,129;72,105,18,14";
      const char *kLastPipeCheckpointSurface =
          "2,0,24,60;2,132,24,108;175,0,24,39;175,111,24,129;72,103,18,14";
      const long kStartTick = 2;
      const long kFlap2Tick = 40;
      const long kFirstPipeTick = 44;
      const long kFlap3Tick = 78;
      const long kFlap4Tick = 116;
      const long kFlap5Tick = 154;
      const long kLastPipeCheckpointTick = 192;
      const long kGameOverTick = 213;
      const long kFinalTick = 273;

      class CheckSurfaceAdapter
      {
      public:
        typedef FloppyBirdScenario::ScenarioInput In;
        typedef FloppyBirdScenario::ScenarioInput Out;

        explicit CheckSurfaceAdapter(const char *expected)
            : expected_(expected ? expected : "")
        {
        }

        dsl::StepRunStatus run(In const &in, Out &out, dsl::FlowError &error) const
        {
          out = in;
          std::string rectangles;
          const dsl::StepRunStatus capture = CaptureRectSurfaceModel(in.scene, kSurface, rectangles, error);
          if (capture != dsl::FLOW_STEP_SUCCEEDED)
          {
            return capture;
          }
          if (rectangles != this->expected_)
          {
            error.kind = dsl::testing::FLOW_ERROR_KIND_SCENE_TEST_ASSERT;
            error.code = dsl::testing::FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED;
            return dsl::FLOW_STEP_FAILED;
          }
          return dsl::FLOW_STEP_SUCCEEDED;
        }

      private:
        std::string expected_;
      };

      CheckSurfaceAdapter CheckSurface(const char *expected)
      {
        return CheckSurfaceAdapter(expected);
      }

      class FlapAdapter
      {
      public:
        typedef FloppyBirdScenario::ScenarioInput In;
        typedef FloppyBirdScenario::ScenarioInput Out;

        dsl::StepRunStatus run(In const &in, Out &out, dsl::FlowError &error) const
        {
          out = in;
          if (!in.game)
          {
            error.kind = dsl::testing::FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = dsl::testing::FLOW_ERROR_SCENE_TEST_INVALID_CAPTURE_VALUE;
            return dsl::FLOW_STEP_FAILED;
          }
          in.game->flap();
          return dsl::FLOW_STEP_SUCCEEDED;
        }
      };

      class CheckGameOverAdapter
      {
      public:
        typedef FloppyBirdScenario::ScenarioInput In;
        typedef FloppyBirdScenario::ScenarioInput Out;

        dsl::StepRunStatus run(In const &in, Out &out, dsl::FlowError &error) const
        {
          out = in;
          if (!in.game)
          {
            error.kind = dsl::testing::FLOW_ERROR_KIND_SCENE_SCENARIO;
            error.code = dsl::testing::FLOW_ERROR_SCENE_TEST_INVALID_CAPTURE_VALUE;
            return dsl::FLOW_STEP_FAILED;
          }
          if (in.game->gameState() != loka_floppy_bird::GAME_DEAD || in.game->score() != 1)
          {
            error.kind = dsl::testing::FLOW_ERROR_KIND_SCENE_TEST_ASSERT;
            error.code = dsl::testing::FLOW_ERROR_SCENE_TEST_ASSERTION_FAILED;
            return dsl::FLOW_STEP_FAILED;
          }
          return dsl::FLOW_STEP_SUCCEEDED;
        }
      };

      class SnapSurfaceAdapter
      {
      public:
        typedef FloppyBirdScenario::ScenarioInput In;
        typedef dsl::SnapRecord Out;

        explicit SnapSurfaceAdapter(long tick)
            : tick_(tick)
        {
        }

        dsl::StepRunStatus run(In const &in, Out &out, dsl::FlowError &error) const
        {
          std::string rectangles;
          const dsl::StepRunStatus capture = CaptureRectSurfaceModel(in.scene, kSurface, rectangles, error);
          if (capture != dsl::FLOW_STEP_SUCCEEDED)
          {
            return capture;
          }
          dsl::BuildSnapV1RecordAdapter base(
              "FloppyBird", kFixedStepFlaps, kSurface, this->tick_, 1, dsl::SnapStatusOk());
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

      private:
        long tick_;
      };

      dsl::FlowChain<FloppyBirdScenario::ScenarioInput, dsl::SnapRecord>
      BuildFixedStepFlapFlow(dsl::testing::ScenarioClock &clock,
                             FloppyBirdScenario::ScenarioInput *input,
                             dsl::SnapRecord *recordOut,
                             dsl::testing::ScenarioAuditSink *audit)
      {
        using namespace dsl::testing;
        return (ScenarioFlow(clock, input).auditTo(audit)
                | AtTick(kStartTick, CheckSurface(kWaitingSurface)).named("verify-waiting-surface")
                | AtTick(kStartTick, FlapAdapter()).named("flap-1")
                | AtTick(kStartTick, CheckSurface(kStartedSurface)).named("verify-seeded-start")
                | AtTick(kFlap2Tick, FlapAdapter()).named("flap-2")
                | AtTick(kFirstPipeTick, CheckSurface(kFirstPipeSurface)).named("verify-seeded-first-pipe")
                | AtTick(kFlap3Tick, FlapAdapter()).named("flap-3")
                | AtTick(kFlap4Tick, FlapAdapter()).named("flap-4")
                | AtTick(kFlap5Tick, CheckSurface(kMidRunSurface)).named("verify-mid-run")
                | AtTick(kFlap5Tick, FlapAdapter()).named("flap-5")
                | AtTick(kLastPipeCheckpointTick, CheckSurface(kLastPipeCheckpointSurface))
                      .named("verify-final-checkpoint")
                | AtTick(kGameOverTick, CheckGameOverAdapter()).named("verify-game-over")
                | AtTick(kFinalTick, CheckGameOverAdapter()).named("verify-game-over-held")
                | AtTick(kFinalTick, SnapSurfaceAdapter(kFinalTick))
                      .named("capture-final-checkpoint")
                      .onSuccess(recordOut))
            .flow();
      }
    } // namespace

    unsigned long FloppyBirdScenarioSeed()
    {
      return 0x13579BDFUL;
    }

    bool IsFloppyBirdScenario(const std::string &name)
    {
      return name == kFixedStepFlaps;
    }

    FloppyBirdScenario::FixedStepFlapState::FixedStepFlapState(dsl::testing::ScenarioAuditSink *audit)
        : clock_(),
          input_(),
          record_(),
          flow_()
    {
      this->flow_.set(BuildFixedStepFlapFlow(this->clock_, &this->input_, &this->record_, audit));
    }

    dsl::FlowRunResult FloppyBirdScenario::FixedStepFlapState::run(long tick,
                                                                   app::scene::Scene *scene,
                                                                   floppybird::GameModel &game)
    {
      this->clock_.advanceTo(tick);
      this->input_.scene = scene;
      this->input_.game = &game;
      return this->flow_.runResult();
    }

    void FloppyBirdScenario::FixedStepFlapState::stop()
    {
      this->flow_.cancel();
      const dsl::FlowRunResult result = this->flow_.runResult();
      assert(result == dsl::FLOW_RUN_CANCELED && "FloppyBird scenario cancellation must be terminal");
      (void)result;
    }

    const dsl::SnapRecord &FloppyBirdScenario::FixedStepFlapState::record() const
    {
      return this->record_;
    }

    FloppyBirdScenario::FloppyBirdScenario(ScenarioCompletionPolicy completionPolicy,
                                           dsl::testing::ScenarioAuditSink *audit)
        : name_(kFixedStepFlaps),
          completionPolicy_(completionPolicy),
          terminalState_(SCENARIO_ADVANCE_PENDING),
          terminalAudit_(audit),
          scenarioState_(audit)
    {
    }

    ScenarioAdvance FloppyBirdScenario::step(long tick,
                                              app::scene::Scene *scene,
                                              floppybird::GameModel &game,
                                              const CaptureContentBounds &bounds,
                                              dsl::SnapRecord &out)
    {
      if (this->terminalState_ != SCENARIO_ADVANCE_PENDING)
      {
        return this->terminalState_;
      }
      const dsl::FlowRunResult result = this->scenarioState_.run(tick, scene, game);
      if (result == dsl::FLOW_RUN_PENDING)
      {
        return SCENARIO_ADVANCE_PENDING;
      }
      if (result != dsl::FLOW_RUN_SUCCEEDED)
      {
        out = MakeFloppyBirdDriverErrorRecord(2701, "scenario expectations were not met");
      }
      else
      {
        out = this->scenarioState_.record();
        out.setInt("seed", static_cast<long>(FloppyBirdScenarioSeed()));
        out.set("fixed_step_seconds", "1/60");
        out.setInt("fixed_step_count", kFinalTick);
        out.setInt("game_over_tick", kGameOverTick);
        out.setInt("game_over_hold_ticks", kFinalTick - kGameOverTick);
        out.set("flap_ticks", "2,40,78,116,154");
        out.set("checkpoint.waiting", kWaitingSurface);
        out.set("checkpoint.started", kStartedSurface);
        out.set("checkpoint.first_pipe", kFirstPipeSurface);
        out.set("checkpoint.mid_run", kMidRunSurface);
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
          out = MakeFloppyBirdDriverErrorRecord(2703, "scenario audit write failed");
        }
        this->terminalState_ = SCENARIO_ADVANCE_FINAL_SCENE_HELD;
        break;
      }
      return this->terminalState_;
    }

    bool FloppyBirdScenario::publishVerdict(const dsl::SnapRecord &record)
    {
      std::string verdict;
      const dsl::testing::ScenarioAuditTerminalStatus status =
          record.get("status", verdict) && verdict == dsl::SnapStatusOk()
              ? dsl::testing::SCENARIO_AUDIT_SUCCEEDED
              : dsl::testing::SCENARIO_AUDIT_FAILED;
      return this->terminalAudit_.emit(status, record);
    }

    const std::string &FloppyBirdScenario::name() const
    {
      return this->name_;
    }

    void FloppyBirdScenario::stop()
    {
      if (this->terminalState_ == SCENARIO_ADVANCE_PENDING)
      {
        this->scenarioState_.stop();
        (void)this->terminalAudit_.emit(dsl::testing::SCENARIO_AUDIT_CANCELED);
        this->terminalState_ = SCENARIO_ADVANCE_FINAL_SCENE_HELD;
      }
    }

    dsl::SnapRecord MakeFloppyBirdDriverErrorRecord(long errorCode, const char *message)
    {
      dsl::SnapRecord record;
      record.setInt("format_version", 1);
      record.setInt("schema_version", 1);
      record.setInt("scenario_version", 1);
      record.set("test", "FloppyBird");
      record.set("step", kFixedStepFlaps);
      record.set("node", kSurface);
      record.setInt("tick", 0);
      record.set("status", dsl::SnapStatusError());
      record.setInt("error_code", errorCode);
      record.set("error_msg", message ? message : "driver error");
      return record;
    }
  } // namespace scenario_tests
} // namespace loka
