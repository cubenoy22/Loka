#include "TutorialScenarioTests.hpp"

#include "support/TestVerify.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../example/Tutorial/src/Step4Node.hpp"
#include "app/scene/Scene.hpp"
#include "core/util/OwnedDef.hpp"
#include "platform/file/FileHandle.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "scenarios/TutorialScenarios.hpp"
#include "testing/scene/ScenarioAudit.hpp"

namespace
{
  class RecordingTutorialAudit : public loka::dsl::testing::ScenarioAuditSink
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
    const std::string unixMarker = "/tests/TutorialScenarioTests.cpp";
    const std::string windowsMarker = "\\tests\\TutorialScenarioTests.cpp";
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

  void VerifyRecordInt(const loka::dsl::SnapRecord &record, const char *key, long expected)
  {
    long actual = -1;
    const bool found = record.getInt(key, actual);
    LOKA_VERIFY(found);
    LOKA_VERIFY(actual == expected);
  }

  loka::app::scene::NodeDefinitionBase *CloneTutorialRoot()
  {
    tutorial::Step4Node::PropsType props;
    loka::app::scene::BoundaryDefinition<tutorial::Step4Node::PropsType, tutorial::Step4Node> definition(props);
    return definition.clone();
  }

  void AdvanceToCompletion(loka::scenario_tests::TutorialScenario &scenario,
                           loka::app::scene::Scene &scene,
                           loka::scenario_tests::CaptureContentBounds &bounds,
                           loka::dsl::SnapRecord &record,
                           loka::scenario_tests::ScenarioAdvance expected)
  {
    LOKA_VERIFY(scenario.step(2, &scene, bounds, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
    LOKA_VERIFY(scenario.step(32, &scene, bounds, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
    LOKA_VERIFY(scenario.step(62, &scene, bounds, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
    LOKA_VERIFY(scenario.step(92, &scene, bounds, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
    LOKA_VERIFY(scenario.step(122, &scene, bounds, record) == expected);
  }
} // namespace

void testTutorialIncrementSummaryToggleDrivesUiCommands()
{
  loka::core::OwnedDef<loka::app::scene::NodeDefinitionBase> root(CloneTutorialRoot());
  LOKA_VERIFY(root.get() != 0);
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(root.take());
  scene.mount(&platform);
  scene.updateAttached(true);

  RecordingTutorialAudit audit;
  loka::scenario_tests::TutorialScenario scenario(loka::scenario_tests::SCENARIO_COMPLETION_DRIVER_OWNED, &audit);
  loka::scenario_tests::CaptureContentBounds bounds;
  bounds.available = true;
  bounds.right = 360;
  bounds.bottom = 280;
  loka::dsl::SnapRecord record;
  AdvanceToCompletion(
      scenario, scene, bounds, record, loka::scenario_tests::SCENARIO_ADVANCE_DRIVER_COMPLETION_READY);
  LOKA_VERIFY(scenario.publishVerdict(record));

  std::string value;
  LOKA_VERIFY(record.get("test", value) && value == "Tutorial");
  LOKA_VERIFY(record.get("step", value) && value == "increment-summary-toggle");
  LOKA_VERIFY(record.get("node", value) && value == "Tutorial.Step4.Summary");
  LOKA_VERIFY(record.get("text.value", value) && value == "Items: 2");
  LOKA_VERIFY(record.get("summary_toggle_round_trip", value) && value == "true");
  VerifyRecordInt(record, "increment_count", 2);
  VerifyRecordInt(record, "tick", 122);
  VerifyRecordInt(record, "crop_right", 360);
  VerifyRecordInt(record, "crop_bottom", 280);
  LOKA_VERIFY(audit.steps.size() == 10);
  LOKA_VERIFY(audit.steps[1].name() == "increment-to-one");
  LOKA_VERIFY(audit.steps[3].name() == "increment-to-two");
  LOKA_VERIFY(audit.steps[5].name() == "hide-summary");
  LOKA_VERIFY(audit.steps[6].name() == "verify-summary-hidden");
  LOKA_VERIFY(audit.steps[7].name() == "show-summary");
  LOKA_VERIFY(audit.terminals.size() == 1);
  LOKA_VERIFY(audit.terminals[0] == loka::dsl::testing::SCENARIO_AUDIT_SUCCEEDED);
  LOKA_VERIFY(audit.verdicts.size() == 1);

  scene.unmount();
  std::printf("testTutorialIncrementSummaryToggleDrivesUiCommands passed\n");
}

void testTutorialIncrementSummaryToggleHoldsFinalSceneAndMatchesAudit()
{
  const char *actualPath = "_loka_tutorial_increment_summary_toggle.audit";
  std::remove(actualPath);
  loka::core::OwnedDef<loka::app::scene::NodeDefinitionBase> root(CloneTutorialRoot());
  LOKA_VERIFY(root.get() != 0);
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(root.take());
  scene.mount(&platform);
  scene.updateAttached(true);

  {
    loka::platform::file::FileHandle destination;
    destination.displayPath = loka::core::String::Utf8(actualPath, std::strlen(actualPath));
    loka::dsl::testing::ScenarioAuditFile audit(destination, "increment-summary-toggle");
    const bool auditValid = audit.isValid();
    LOKA_VERIFY(auditValid);
    loka::scenario_tests::TutorialScenario scenario(loka::scenario_tests::SCENARIO_COMPLETION_HOLD_FINAL_SCENE,
                                                     &audit);
    loka::scenario_tests::CaptureContentBounds bounds;
    bounds.available = true;
    bounds.right = 360;
    bounds.bottom = 280;
    loka::dsl::SnapRecord record;
    AdvanceToCompletion(scenario, scene, bounds, record, loka::scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD);
    LOKA_VERIFY(scenario.step(123, &scene, bounds, record)
                == loka::scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD);
  }

  const std::string actual = ReadBytes(actualPath);
  const std::string expectedPath =
      SourcePath("tests/scenarios/expected/tutorial/increment-summary-toggle.audit");
  const std::string expected = ReadBytes(expectedPath.c_str());
  LOKA_VERIFY(actual == expected);
  std::remove(actualPath);

  scene.unmount();
  std::printf("testTutorialIncrementSummaryToggleHoldsFinalSceneAndMatchesAudit passed\n");
}
