#include "MineSweeperScenarioTests.hpp"

#include "support/TestVerify.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../example/MineSweeper/src/MainNode.hpp"
#include "app/scene/Scene.hpp"
#include "core/util/OwnedDef.hpp"
#include "platform/file/FileHandle.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "scenarios/MineSweeperScenarios.hpp"
#include "testing/scene/ScenarioAudit.hpp"

namespace
{
  class RecordingMineSweeperAudit : public loka::dsl::testing::ScenarioAuditSink
  {
  public:
    virtual bool recordStep(const loka::dsl::testing::ScenarioStepTerminal &record)
    {
      this->steps.push_back(record);
      return true;
    }

    virtual bool recordTerminal(loka::dsl::testing::ScenarioAuditTerminalStatus status)
    {
      this->terminals.push_back(status);
      return true;
    }

    virtual bool recordVerdict(const loka::dsl::SnapRecord &record)
    {
      this->verdicts.push_back(record);
      return true;
    }

    std::vector<loka::dsl::testing::ScenarioStepTerminal> steps;
    std::vector<loka::dsl::SnapRecord> verdicts;
    std::vector<loka::dsl::testing::ScenarioAuditTerminalStatus> terminals;
  };

  std::string SourcePath(const char *relative)
  {
    const std::string sourceFile(__FILE__);
    const std::string unixMarker = "/tests/MineSweeperScenarioTests.cpp";
    const std::string windowsMarker = "\\tests\\MineSweeperScenarioTests.cpp";
    std::string::size_type marker = sourceFile.rfind(unixMarker);
    if (marker == std::string::npos)
    {
      marker = sourceFile.rfind(windowsMarker);
    }
    assert(marker != std::string::npos);
    return sourceFile.substr(0, marker) + "/" + (relative ? relative : "");
  }

  std::string ReadBytes(const char *path)
  {
    std::string content;
    FILE *input = std::fopen(path, "rb");
    LOKA_VERIFY(input != 0);
    if (!input)
    {
      return content;
    }
    char buffer[512];
    std::size_t count = 0;
    while ((count = std::fread(buffer, 1, sizeof(buffer), input)) != 0)
    {
      content.append(buffer, count);
    }
    LOKA_VERIFY(std::fclose(input) == 0);
    return content;
  }

  loka::app::scene::NodeDefinitionBase *CloneMineSweeperRoot(unsigned long seed)
  {
    const minesweeper::MainProps props(seed);
    loka::app::scene::BoundaryDefinition<minesweeper::MainProps, minesweeper::MainNode> definition(props);
    return definition.clone();
  }

  void AdvanceToCompletion(loka::scenario_tests::MineSweeperScenario &scenario,
                           loka::app::scene::Scene &scene,
                           loka::scenario_tests::CaptureContentBounds &bounds,
                           loka::dsl::SnapRecord &record,
                           loka::scenario_tests::ScenarioAdvance expected)
  {
    LOKA_VERIFY(scenario.step(2, &scene, bounds, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
    LOKA_VERIFY(scenario.step(32, &scene, bounds, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
    LOKA_VERIFY(scenario.step(62, &scene, bounds, record) == expected);
  }

  void VerifyRecordString(const loka::dsl::SnapRecord &record, const char *key, const char *expected)
  {
    std::string value;
    LOKA_VERIFY(record.get(key, value));
    LOKA_VERIFY(value == expected);
  }
} // namespace

void testMineSweeperNewGameTwiceDrivesOwnerEmitter()
{
  loka::core::OwnedDef<loka::app::scene::NodeDefinitionBase> root(
      CloneMineSweeperRoot(loka::scenario_tests::MineSweeperScenarioSeed()));
  LOKA_VERIFY(root.get() != 0);
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(root.take());
  scene.mount(&platform);
  scene.updateAttached(true);

  RecordingMineSweeperAudit audit;
  loka::scenario_tests::MineSweeperScenario scenario(
      loka::scenario_tests::SCENARIO_COMPLETION_DRIVER_OWNED, &audit);
  loka::scenario_tests::CaptureContentBounds bounds;
  bounds.available = true;
  bounds.right = 220;
  bounds.bottom = 240;
  loka::dsl::SnapRecord record;
  AdvanceToCompletion(
      scenario, scene, bounds, record, loka::scenario_tests::SCENARIO_ADVANCE_DRIVER_COMPLETION_READY);
  LOKA_VERIFY(scenario.publishVerdict(record));

  VerifyRecordString(record, "test", "MineSweeper");
  VerifyRecordString(record, "step", "new-game-twice");
  VerifyRecordString(record, "node", "MineSweeper.Board");
  VerifyRecordString(record, "board.initial_mines", "3,4,20,22,37,45,50,55,56,59");
  VerifyRecordString(record, "board.after_new_game_1_mines", "1,6,16,23,27,30,34,44,45,53");
  VerifyRecordString(record, "board.after_new_game_2_mines", "3,26,42,49,50,56,59,60,62,63");
  VerifyRecordString(record, "board.mines", "3,26,42,49,50,56,59,60,62,63");
  LOKA_VERIFY(audit.steps.size() == 6);
  LOKA_VERIFY(audit.steps[1].name() == "start-first-new-game");
  LOKA_VERIFY(audit.steps[3].name() == "start-second-new-game");
  LOKA_VERIFY(audit.terminals.size() == 1);
  LOKA_VERIFY(audit.terminals[0] == loka::dsl::testing::SCENARIO_AUDIT_SUCCEEDED);
  LOKA_VERIFY(audit.verdicts.size() == 1);

  scene.unmount();
  std::printf("testMineSweeperNewGameTwiceDrivesOwnerEmitter passed\n");
}

void testMineSweeperNewGameTwiceHoldsFinalSceneAndMatchesAudit()
{
  const char *actualPath = "_loka_minesweeper_new_game_twice.audit";
  std::remove(actualPath);
  loka::core::OwnedDef<loka::app::scene::NodeDefinitionBase> root(
      CloneMineSweeperRoot(loka::scenario_tests::MineSweeperScenarioSeed()));
  LOKA_VERIFY(root.get() != 0);
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(root.take());
  scene.mount(&platform);
  scene.updateAttached(true);

  {
    loka::platform::file::FileHandle destination;
    destination.displayPath = loka::core::String::Utf8(actualPath, std::strlen(actualPath));
    loka::dsl::testing::ScenarioAuditFile audit(destination, "new-game-twice");
    LOKA_VERIFY(audit.isValid());
    loka::scenario_tests::MineSweeperScenario scenario(
        loka::scenario_tests::SCENARIO_COMPLETION_HOLD_FINAL_SCENE, &audit);
    loka::scenario_tests::CaptureContentBounds bounds;
    bounds.available = true;
    bounds.right = 220;
    bounds.bottom = 240;
    loka::dsl::SnapRecord record;
    AdvanceToCompletion(
        scenario, scene, bounds, record, loka::scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD);
    LOKA_VERIFY(scenario.step(63, &scene, bounds, record)
                == loka::scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD);
  }

  const std::string actual = ReadBytes(actualPath);
  const std::string expectedPath =
      SourcePath("tests/scenarios/expected/minesweeper/new-game-twice.audit");
  const std::string expected = ReadBytes(expectedPath.c_str());
  LOKA_VERIFY(actual == expected);
  std::remove(actualPath);

  scene.unmount();
  std::printf("testMineSweeperNewGameTwiceHoldsFinalSceneAndMatchesAudit passed\n");
}

void testMineSweeperDifferentSeedRefusesFixedBoardAudit()
{
  const char *actualPath = "_loka_minesweeper_different_seed.audit";
  std::remove(actualPath);
  loka::core::OwnedDef<loka::app::scene::NodeDefinitionBase> root(CloneMineSweeperRoot(0x2468ACE0UL));
  LOKA_VERIFY(root.get() != 0);
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(root.take());
  scene.mount(&platform);
  scene.updateAttached(true);

  {
    loka::platform::file::FileHandle destination;
    destination.displayPath = loka::core::String::Utf8(actualPath, std::strlen(actualPath));
    loka::dsl::testing::ScenarioAuditFile audit(destination, "new-game-twice");
    LOKA_VERIFY(audit.isValid());
    loka::scenario_tests::MineSweeperScenario scenario(
        loka::scenario_tests::SCENARIO_COMPLETION_HOLD_FINAL_SCENE, &audit);
    loka::scenario_tests::CaptureContentBounds bounds;
    loka::dsl::SnapRecord record;
    LOKA_VERIFY(scenario.step(2, &scene, bounds, record)
                == loka::scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD);
    VerifyRecordString(record, "status", loka::dsl::SnapStatusError());
  }

  const std::string actual = ReadBytes(actualPath);
  const std::string expectedPath =
      SourcePath("tests/scenarios/expected/minesweeper/new-game-twice.audit");
  const std::string expected = ReadBytes(expectedPath.c_str());
  LOKA_VERIFY(actual != expected);
  LOKA_VERIFY(actual.find("status=failed") != std::string::npos);
  LOKA_VERIFY(actual.find("terminal status=failed") != std::string::npos);
  std::remove(actualPath);

  scene.unmount();
  std::printf("testMineSweeperDifferentSeedRefusesFixedBoardAudit passed\n");
}
