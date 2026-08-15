#include "MineSweeperScenarios.hpp"

#include <cassert>
#include <cstdio>

#include "../../example/MineSweeper/src/MainNode.hpp"

#if !defined(TEST_BUILD)
#error MineSweeper scenarios require TEST_BUILD
#endif

namespace loka
{
  namespace scenario_tests
  {
    namespace
    {
      const char *kNewGameTwice = "new-game-twice";
      const char *kBoard = "MineSweeper.Board";
      const char *kNewGameButton = "MineSweeper.NewGameButton";
      const char *kInitialMines = "3,4,20,22,37,45,50,55,56,59";
      const char *kFirstNewGameMines = "1,6,16,23,27,30,34,44,45,53";
      const char *kSecondNewGameMines = "3,26,42,49,50,56,59,60,62,63";
      const long kInitialTick = 2;
      const long kStepSpacingTicks = 5;

      void CollectMineCoordinates(app::scene::Node *node,
                                  int &cellIndex,
                                  int &mineCount,
                                  std::string &coordinates)
      {
        if (!node)
        {
          return;
        }
        if (node->propsTypeId() == minesweeper::MineCellProps::staticTypeId())
        {
          minesweeper::MineCellNode *cell = static_cast<minesweeper::MineCellNode *>(node);
          if (cell->props.isMine)
          {
            if (!coordinates.empty())
            {
              coordinates += ',';
            }
            char indexText[16];
            ::snprintf(indexText, sizeof(indexText), "%d", cellIndex);
            coordinates += indexText;
            ++mineCount;
          }
          ++cellIndex;
        }
        app::scene::INestable *nestable = node->asNestable();
        for (app::scene::Node *child = nestable ? nestable->childrenHead() : 0;
             child;
             child = child->nextInComposition)
        {
          CollectMineCoordinates(child, cellIndex, mineCount, coordinates);
        }
      }

      dsl::StepRunStatus CaptureMineCoordinates(app::scene::Scene *scene,
                                                std::string &coordinates,
                                                dsl::FlowError &error)
      {
        app::scene::Node *boardNode = 0;
        const dsl::StepRunStatus lookup =
            dsl::testing::LookupNodeById<app::scene::Node>(scene, kBoard, boardNode, error);
        if (lookup != dsl::FLOW_STEP_SUCCEEDED)
        {
          return lookup;
        }
        app::GridNode *board = boardNode ? boardNode->asGridNode() : 0;
        if (!board)
        {
          error.kind = dsl::testing::FLOW_ERROR_KIND_SCENE_SCENARIO;
          error.code = dsl::testing::FLOW_ERROR_SCENE_TEST_NODE_TYPE_MISMATCH;
          return dsl::FLOW_STEP_FAILED;
        }
        coordinates.clear();
        int cellCount = 0;
        int mineCount = 0;
        CollectMineCoordinates(board, cellCount, mineCount, coordinates);
        if (cellCount != 64 || mineCount != 10)
        {
          error.kind = dsl::testing::FLOW_ERROR_KIND_SCENE_SCENARIO;
          error.code = dsl::testing::FLOW_ERROR_SCENE_TEST_INVALID_CAPTURE_VALUE;
          return dsl::FLOW_STEP_FAILED;
        }
        return dsl::FLOW_STEP_SUCCEEDED;
      }

      class CheckMineSweeperBoardAdapter
      {
      public:
        typedef app::scene::Scene *In;
        typedef app::scene::Scene *Out;

        explicit CheckMineSweeperBoardAdapter(const char *expected)
            : expected_(expected ? expected : "")
        {
        }

        dsl::StepRunStatus run(In const &in, Out &out, dsl::FlowError &error) const
        {
          out = in;
          std::string coordinates;
          const dsl::StepRunStatus capture = CaptureMineCoordinates(in, coordinates, error);
          if (capture != dsl::FLOW_STEP_SUCCEEDED)
          {
            return capture;
          }
          if (coordinates != this->expected_)
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

      CheckMineSweeperBoardAdapter CheckMineSweeperBoard(const char *expected)
      {
        return CheckMineSweeperBoardAdapter(expected);
      }

      class SnapMineSweeperBoardAdapter
      {
      public:
        typedef app::scene::Scene *In;
        typedef dsl::SnapRecord Out;

        SnapMineSweeperBoardAdapter(long tick)
            : tick_(tick)
        {
        }

        dsl::StepRunStatus run(In const &in, Out &out, dsl::FlowError &error) const
        {
          std::string coordinates;
          const dsl::StepRunStatus capture = CaptureMineCoordinates(in, coordinates, error);
          if (capture != dsl::FLOW_STEP_SUCCEEDED)
          {
            return capture;
          }
          dsl::BuildSnapV1RecordAdapter base(
              "MineSweeper", kNewGameTwice, kBoard, this->tick_, 1, dsl::SnapStatusOk());
          const int unused = 0;
          dsl::FlowError ignored;
          if (base.run(unused, out, ignored) != dsl::FLOW_STEP_SUCCEEDED)
          {
            error.kind = dsl::FLOW_ERROR_KIND_SNAP;
            error.code = dsl::FLOW_ERROR_SNAP_WRITE_FAILED;
            return dsl::FLOW_STEP_FAILED;
          }
          out.set("board.mines", coordinates.c_str());
          return dsl::FLOW_STEP_SUCCEEDED;
        }

      private:
        long tick_;
      };

      dsl::FlowChain<app::scene::Scene *, dsl::SnapRecord>
      BuildNewGameTwiceFlow(dsl::testing::ScenarioClock &clock,
                            app::scene::Scene **sceneInput,
                            dsl::SnapRecord *recordOut,
                            dsl::testing::ScenarioAuditSink *audit)
      {
        using namespace dsl::testing;
        const long firstNewGameTick = kInitialTick + kStepSpacingTicks;
        const long secondNewGameTick = firstNewGameTick + kStepSpacingTicks;
        return (ScenarioFlow(clock, sceneInput).auditTo(audit)
                | AtTick(kInitialTick, CheckMineSweeperBoard(kInitialMines)).named("verify-initial-board")
                | AtTick(kInitialTick, ClickButton(kNewGameButton)).named("start-first-new-game")
                | AtTick(firstNewGameTick, CheckMineSweeperBoard(kFirstNewGameMines))
                      .named("verify-first-new-game-board")
                | AtTick(firstNewGameTick, ClickButton(kNewGameButton)).named("start-second-new-game")
                | AtTick(secondNewGameTick, CheckMineSweeperBoard(kSecondNewGameMines))
                      .named("verify-second-new-game-board")
                | AtTick(secondNewGameTick, SnapMineSweeperBoardAdapter(secondNewGameTick))
                      .named("capture-second-new-game-board")
                      .onSuccess(recordOut))
            .flow();
      }
    } // namespace

    unsigned long MineSweeperScenarioSeed()
    {
      return 0x13579BDFUL;
    }

    bool IsMineSweeperScenario(const std::string &name)
    {
      return name == kNewGameTwice;
    }

    MineSweeperScenario::NewGameTwiceState::NewGameTwiceState(dsl::testing::ScenarioAuditSink *audit)
        : clock_(),
          scene_(0),
          record_(),
          flow_()
    {
      this->flow_.set(BuildNewGameTwiceFlow(this->clock_, &this->scene_, &this->record_, audit));
    }

    dsl::FlowRunResult MineSweeperScenario::NewGameTwiceState::run(long tick, app::scene::Scene *scene)
    {
      this->clock_.advanceTo(tick);
      this->scene_ = scene;
      return this->flow_.runResult();
    }

    void MineSweeperScenario::NewGameTwiceState::stop()
    {
      this->flow_.cancel();
      const dsl::FlowRunResult result = this->flow_.runResult();
      assert(result == dsl::FLOW_RUN_CANCELED && "MineSweeper scenario cancellation must be terminal");
      (void)result;
    }

    const dsl::SnapRecord &MineSweeperScenario::NewGameTwiceState::record() const
    {
      return this->record_;
    }

    MineSweeperScenario::MineSweeperScenario(ScenarioCompletionPolicy completionPolicy,
                                             dsl::testing::ScenarioAuditSink *audit)
        : name_(kNewGameTwice),
          completionPolicy_(completionPolicy),
          terminalState_(SCENARIO_ADVANCE_PENDING),
          terminalAudit_(audit),
          scenarioState_(audit)
    {
    }

    ScenarioAdvance MineSweeperScenario::step(long tick,
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
        out = MakeMineSweeperDriverErrorRecord(2601, "scenario expectations were not met");
      }
      else
      {
        out = this->scenarioState_.record();
        out.setInt("seed", static_cast<long>(MineSweeperScenarioSeed()));
        out.setInt("new_game_count", 2);
        out.set("board.initial_mines", kInitialMines);
        out.set("board.after_new_game_1_mines", kFirstNewGameMines);
        out.set("board.after_new_game_2_mines", kSecondNewGameMines);
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
          out = MakeMineSweeperDriverErrorRecord(2603, "scenario audit write failed");
        }
        this->terminalState_ = SCENARIO_ADVANCE_FINAL_SCENE_HELD;
        break;
      }
      return this->terminalState_;
    }

    bool MineSweeperScenario::publishVerdict(const dsl::SnapRecord &record)
    {
      std::string verdict;
      const dsl::testing::ScenarioAuditTerminalStatus status =
          record.get("status", verdict) && verdict == dsl::SnapStatusOk()
              ? dsl::testing::SCENARIO_AUDIT_SUCCEEDED
              : dsl::testing::SCENARIO_AUDIT_FAILED;
      return this->terminalAudit_.emit(status, record);
    }

    const std::string &MineSweeperScenario::name() const
    {
      return this->name_;
    }

    void MineSweeperScenario::stop()
    {
      if (this->terminalState_ == SCENARIO_ADVANCE_PENDING)
      {
        this->scenarioState_.stop();
        (void)this->terminalAudit_.emit(dsl::testing::SCENARIO_AUDIT_CANCELED);
        this->terminalState_ = SCENARIO_ADVANCE_FINAL_SCENE_HELD;
      }
    }

    dsl::SnapRecord MakeMineSweeperDriverErrorRecord(long errorCode, const char *message)
    {
      dsl::SnapRecord record;
      record.setInt("format_version", 1);
      record.setInt("schema_version", 1);
      record.setInt("scenario_version", 1);
      record.set("test", "MineSweeper");
      record.set("step", kNewGameTwice);
      record.set("node", kBoard);
      record.setInt("tick", 0);
      record.set("status", dsl::SnapStatusError());
      record.setInt("error_code", errorCode);
      record.set("error_msg", message ? message : "driver error");
      return record;
    }
  } // namespace scenario_tests
} // namespace loka
