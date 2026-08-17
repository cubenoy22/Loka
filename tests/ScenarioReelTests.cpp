#include "ScenarioReelTests.hpp"

#include "support/TestVerify.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "../example/HelloWorld/src/MainNode.hpp"
#include "../example/MineSweeper/src/MainNode.hpp"
#include "app/core/Window.hpp"
#include "app/scene/Scene.hpp"
#include "core/util/OwnedDef.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "platform/null/NullWindow.hpp"
#include "scenarios/HelloWorldScenarios.hpp"
#include "scenarios/MineSweeperScenarios.hpp"
#include "scenarios/ObservedMainDefinition.hpp"
#include "scenarios/ScenarioReel.hpp"
#include "scenarios/StartupScenarios.hpp"

namespace
{
  const char *const kProbeCells[] = {"startup", "first", "second"};

  loka::scenario_tests::ScenarioCellTable ProbeCells()
  {
    return loka::scenario_tests::ScenarioCellTable(kProbeCells, 3);
  }

  std::string SourcePath(const char *relative)
  {
    const std::string sourceFile(__FILE__);
    const std::string unixMarker = "/tests/ScenarioReelTests.cpp";
    const std::string windowsMarker = "\\tests\\ScenarioReelTests.cpp";
    std::string::size_type marker = sourceFile.rfind(unixMarker);
    if (marker == std::string::npos)
    {
      marker = sourceFile.rfind(windowsMarker);
    }
    assert(marker != std::string::npos); // loka-assert-ok: fixture path derivation
    return sourceFile.substr(0, marker) + "/" + (relative ? relative : "");
  }

  /** The cells scenarios.txt registers for one example, in file order. */
  std::vector<std::string> RegistryCells(const char *example)
  {
    std::vector<std::string> cells;
    const std::string path = SourcePath("tests/scenarios/scenarios.txt");
    std::FILE *input = std::fopen(path.c_str(), "rb");
    LOKA_VERIFY(input != 0);
    if (!input)
    {
      return cells;
    }
    char line[256];
    while (std::fgets(line, sizeof(line), input))
    {
      std::string entry(line);
      const std::string::size_type end = entry.find_last_not_of("\r\n");
      entry = end == std::string::npos ? std::string() : entry.substr(0, end + 1);
      const std::string::size_type space = entry.find(' ');
      if (space == std::string::npos)
      {
        continue;
      }
      if (entry.substr(0, space) == example)
      {
        cells.push_back(entry.substr(space + 1));
      }
    }
    LOKA_VERIFY(std::fclose(input) == 0);
    return cells;
  }

  void VerifyTableMatchesRegistry(const char *example, const loka::scenario_tests::ScenarioCellTable &table)
  {
    const std::vector<std::string> registered = RegistryCells(example);
    LOKA_VERIFY(!registered.empty());
    LOKA_VERIFY(table.size() == registered.size());
    // The reel opens with the shared startup cell so every pass begins on the
    // example's settled initial screen.
    LOKA_VERIFY(table.at(0) != 0);
    LOKA_VERIFY(loka::scenario_tests::IsStartupScenario(table.at(0)));
    for (std::size_t i = 0; i < registered.size(); ++i)
    {
      LOKA_VERIFY(table.contains(registered[i]));
    }
  }

  loka::app::scene::NodeDefinitionBase *CloneMineSweeperRoot()
  {
    const minesweeper::MainProps props(loka::scenario_tests::MineSweeperScenarioSeed());
    loka::scenario_tests::ObservedMainDefinition<minesweeper::MainProps, minesweeper::MainNode> definition(props, 0);
    return definition.clone();
  }

  loka::app::scene::NodeDefinitionBase *CloneHelloWorldRoot()
  {
    const helloworld::MainProps props;
    loka::scenario_tests::ObservedMainDefinition<helloworld::MainProps, helloworld::MainNode> definition(props, 0);
    return definition.clone();
  }

  /** Runs one MineSweeper new-game-twice rail against a mounted window and
      returns the board it settled on. */
  std::string RunMineSweeperBoardCell(Window &window)
  {
    loka::scenario_tests::MineSweeperScenario scenario(loka::scenario_tests::SCENARIO_COMPLETION_DRIVER_OWNED, 0);
    loka::scenario_tests::CaptureContentBounds bounds;
    loka::dsl::SnapRecord record;
    loka::scenario_tests::ScenarioAdvance advance = loka::scenario_tests::SCENARIO_ADVANCE_PENDING;
    for (long tick = 1; tick <= 64 && advance == loka::scenario_tests::SCENARIO_ADVANCE_PENDING; ++tick)
    {
      LOKA_VERIFY(window.scene() != 0);
      advance = scenario.step(tick, window.scene(), bounds, record);
    }
    LOKA_VERIFY(advance == loka::scenario_tests::SCENARIO_ADVANCE_DRIVER_COMPLETION_READY);
    std::string board;
    LOKA_VERIFY(record.get("board.mines", board));
    return board;
  }

  void CollectMineIndices(loka::app::scene::Node *node, int &cellIndex, std::string &out)
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
        if (!out.empty())
        {
          out += ',';
        }
        char indexText[16];
        std::sprintf(indexText, "%d", cellIndex);
        out += indexText;
      }
      ++cellIndex;
    }
    loka::app::scene::INestable *nestable = node->asNestable();
    for (loka::app::scene::Node *child = nestable ? nestable->childrenHead() : 0;
         child;
         child = child->nextInComposition)
    {
      CollectMineIndices(child, cellIndex, out);
    }
  }

  /** The board a MineSweeper scene is currently showing, read the way the rail
      reads it: off the composed MineCell props, not off any reel bookkeeping. */
  std::string SceneBoard(loka::app::scene::Scene *scene)
  {
    std::string mines;
    if (!scene)
    {
      return mines;
    }
    loka::app::scene::Node *boardNode = 0;
    loka::dsl::FlowError error;
    if (loka::dsl::testing::LookupNodeById<loka::app::scene::Node>(scene, "MineSweeper.Board", boardNode, error)
        != loka::dsl::FLOW_STEP_SUCCEEDED)
    {
      return mines;
    }
    int cellIndex = 0;
    CollectMineIndices(boardNode, cellIndex, mines);
    return mines;
  }

  /** What one reel tick is worth recording. Two flavours because one example
      can be watched by name alone and the other has a board worth comparing. */
  typedef std::string (*ReelFrameFn)(const char *cell, Window &window);

  std::string CellFrame(const char *cell, Window &window)
  {
    (void)window;
    return std::string(cell ? cell : "");
  }

  std::string CellAndBoardFrame(const char *cell, Window &window)
  {
    return std::string(cell ? cell : "") + "|" + SceneBoard(window.scene());
  }

  /** Pumps a reel until it retires its bounded cycle budget, recording every
      distinct frame it passes through so one pass can be compared to the next.
      Sampling happens before the tick, so the reel's retirement never lands an
      odd trailing frame in the sequence. */
  template <class InteractionScenario>
  std::vector<std::string> PumpReel(loka::scenario_tests::ScenarioReel<InteractionScenario> &reel,
                                    Window &window,
                                    long maxTicks,
                                    ReelFrameFn frame)
  {
    std::vector<std::string> frames;
    for (long tick = 0; tick < maxTicks && !reel.finished(); ++tick)
    {
      const std::string current = frame(reel.cell(), window);
      if (frames.empty() || frames[frames.size() - 1] != current)
      {
        frames.push_back(current);
      }
      reel.tick(&window, 0.1);
    }
    return frames;
  }
} // namespace

void testScenarioReelPositionWrapsAndCountsCycles()
{
  loka::scenario_tests::ScenarioReelPosition position(ProbeCells(), 2);

  LOKA_VERIFY(!position.exhausted());
  LOKA_VERIFY(std::string(position.cell()) == "startup");
  LOKA_VERIFY(position.completedCycles() == 0);

  position.advance();
  LOKA_VERIFY(std::string(position.cell()) == "first");
  position.advance();
  LOKA_VERIFY(std::string(position.cell()) == "second");
  LOKA_VERIFY(position.completedCycles() == 0);

  // The wrap is what counts a cycle, so the reel is endless by construction
  // rather than by a separate "keep going" flag.
  position.advance();
  LOKA_VERIFY(std::string(position.cell()) == "startup");
  LOKA_VERIFY(position.completedCycles() == 1);
  LOKA_VERIFY(!position.exhausted());

  position.advance();
  position.advance();
  position.advance();
  LOKA_VERIFY(position.completedCycles() == 2);
  LOKA_VERIFY(position.exhausted());
  LOKA_VERIFY(position.cell() == 0);

  // A budget of 0 is the shipping case: it never retires.
  loka::scenario_tests::ScenarioReelPosition endless(ProbeCells(), 0);
  for (int i = 0; i < 64; ++i)
  {
    endless.advance();
  }
  LOKA_VERIFY(!endless.exhausted());
  LOKA_VERIFY(endless.completedCycles() > 0);

  // An empty table never claims to have a cell to run.
  loka::scenario_tests::ScenarioReelPosition empty(loka::scenario_tests::ScenarioCellTable(0, 0), 0);
  LOKA_VERIFY(empty.exhausted());
  LOKA_VERIFY(empty.cell() == 0);

  std::printf("testScenarioReelPositionWrapsAndCountsCycles passed\n");
}

void testScenarioReelCellTablesMatchSharedRegistry()
{
  VerifyTableMatchesRegistry("minesweeper", loka::scenario_tests::MineSweeperReelCells());
  VerifyTableMatchesRegistry("helloworld", loka::scenario_tests::HelloWorldReelCells());

  // The registration predicate answers from the same table, so a cell cannot
  // be registered without joining the reel.
  LOKA_VERIFY(loka::scenario_tests::IsMineSweeperScenario("new-game-twice"));
  LOKA_VERIFY(loka::scenario_tests::IsMineSweeperScenario("seeded-reveal"));
  LOKA_VERIFY(!loka::scenario_tests::IsMineSweeperScenario("startup"));
  LOKA_VERIFY(loka::scenario_tests::IsHelloWorldScenario("toggle-action-probe"));
  LOKA_VERIFY(loka::scenario_tests::IsHelloWorldScenario("bmi-roundtrip"));
  LOKA_VERIFY(!loka::scenario_tests::IsHelloWorldScenario("startup"));

  std::printf("testScenarioReelCellTablesMatchSharedRegistry passed\n");
}

void testScenarioSceneRearmRebuildsExampleStateFromDefinition()
{
  loka::core::OwnedDef<loka::app::scene::NodeDefinitionBase> root(CloneMineSweeperRoot());
  LOKA_VERIFY(root.get() != 0);
  NullPlatformContext context;
  WindowProps windowProps;
  windowProps.scene(new loka::app::scene::Scene(root.take()));
  NullWindow window(&context, windowProps);
  LOKA_VERIFY(window.scene() != 0);
  window.scene()->updateAttached(true);
  const std::size_t mountedNodeCount = window.scene()->liveNodeCount();
  LOKA_VERIFY(mountedNodeCount > 0);

  // Two New Games leave the board somewhere the seed's initial board is not,
  // so a re-arm that only rewound bookkeeping would fail the next rail's
  // verify-initial-board step instead of reproducing this board.
  const std::string firstBoard = RunMineSweeperBoardCell(window);

  LOKA_VERIFY(loka::scenario_tests::RearmScenarioScene(&window));
  LOKA_VERIFY(window.scene() != 0);
  LOKA_VERIFY(window.scene()->liveNodeCount() == mountedNodeCount);

  const std::string secondBoard = RunMineSweeperBoardCell(window);
  LOKA_VERIFY(secondBoard == firstBoard);

  // A window without a mounted scene declines instead of substituting one.
  LOKA_VERIFY(!loka::scenario_tests::RearmScenarioScene(0));

  std::printf("testScenarioSceneRearmRebuildsExampleStateFromDefinition passed\n");
}

void testScenarioReelRunsEveryMineSweeperCellEveryCycle()
{
  loka::core::OwnedDef<loka::app::scene::NodeDefinitionBase> root(CloneMineSweeperRoot());
  LOKA_VERIFY(root.get() != 0);
  NullPlatformContext context;
  WindowProps windowProps;
  windowProps.scene(new loka::app::scene::Scene(root.take()));
  NullWindow window(&context, windowProps);
  LOKA_VERIFY(window.scene() != 0);
  window.scene()->updateAttached(true);
  const std::size_t mountedNodeCount = window.scene()->liveNodeCount();

  loka::scenario_tests::ScenarioReel<loka::scenario_tests::MineSweeperScenario> reel(
      loka::scenario_tests::MineSweeperReelCells(),
      loka::scenario_tests::STARTUP_EXAMPLE_MINESWEEPER,
      &loka::scenario_tests::MakeMineSweeperDriverErrorRecord,
      2602,
      0.05,
      2);

  const std::vector<std::string> frames = PumpReel(reel, window, 4096, &CellAndBoardFrame);

  LOKA_VERIFY(reel.finished());
  LOKA_VERIFY(reel.completedCycles() == 2);
  // The reel must not merely advance: a device left running has to replay the
  // same reel, so the boards the second pass puts on screen have to be the
  // boards the first pass put there. A re-arm that skipped the real teardown
  // would keep whichever board the previous cell finished on and diverge here.
  LOKA_VERIFY(!frames.empty());
  LOKA_VERIFY(frames.size() % 2 == 0);
  const std::size_t half = frames.size() / 2;
  for (std::size_t i = 0; i < half; ++i)
  {
    LOKA_VERIFY(frames[i] == frames[half + i]);
  }
  // The first pass really did visit all three cells in registry order.
  LOKA_VERIFY(frames[0].find("startup|") == 0);
  bool sawNewGameTwice = false;
  bool sawSeededReveal = false;
  for (std::size_t i = 0; i < half; ++i)
  {
    sawNewGameTwice = sawNewGameTwice || frames[i].find("new-game-twice|") == 0;
    sawSeededReveal = sawSeededReveal || frames[i].find("seeded-reveal|") == 0;
  }
  LOKA_VERIFY(sawNewGameTwice);
  LOKA_VERIFY(sawSeededReveal);
  // Five re-arms happened across those two passes and the tree is the size it
  // was at mount, so the reel is rebuilding rather than accumulating.
  LOKA_VERIFY(window.scene()->liveNodeCount() == mountedNodeCount);

  std::printf("testScenarioReelRunsEveryMineSweeperCellEveryCycle passed\n");
}

void testScenarioReelRunsEveryHelloWorldCellEveryCycle()
{
  loka::core::OwnedDef<loka::app::scene::NodeDefinitionBase> root(CloneHelloWorldRoot());
  LOKA_VERIFY(root.get() != 0);
  NullPlatformContext context;
  WindowProps windowProps;
  windowProps.scene(new loka::app::scene::Scene(root.take()));
  NullWindow window(&context, windowProps);
  LOKA_VERIFY(window.scene() != 0);
  window.scene()->updateAttached(true);
  const std::size_t mountedNodeCount = window.scene()->liveNodeCount();

  loka::scenario_tests::ScenarioReel<loka::scenario_tests::HelloWorldScenario> reel(
      loka::scenario_tests::HelloWorldReelCells(),
      loka::scenario_tests::STARTUP_EXAMPLE_HELLO_WORLD,
      &loka::scenario_tests::MakeHelloWorldDriverErrorRecord,
      2402,
      0.05,
      2);

  const std::vector<std::string> visited = PumpReel(reel, window, 4096, &CellFrame);

  LOKA_VERIFY(reel.finished());
  LOKA_VERIFY(reel.completedCycles() == 2);
  LOKA_VERIFY(visited.size() == 6);
  LOKA_VERIFY(visited[0] == "startup");
  LOKA_VERIFY(visited[1] == "toggle-action-probe");
  LOKA_VERIFY(visited[2] == "bmi-roundtrip");
  LOKA_VERIFY(visited[3] == "startup");
  LOKA_VERIFY(visited[4] == "toggle-action-probe");
  LOKA_VERIFY(visited[5] == "bmi-roundtrip");
  LOKA_VERIFY(window.scene()->liveNodeCount() == mountedNodeCount);

  std::printf("testScenarioReelRunsEveryHelloWorldCellEveryCycle passed\n");
}
