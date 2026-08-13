#include "ScrapbookScenarioTests.hpp"

#include "support/TestVerify.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "../example/ScrapbookUI/src/MainNode.hpp"
#include "app/scene/Scene.hpp"
#include "core/io/File.hpp"
#include "core/resource/Image.hpp"
#include "core/util/OwnedDef.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "platform/file/FileHandle.hpp"
#include "scenarios/ScrapbookScenarios.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "testing/scene/ScenarioAudit.hpp"

namespace
{
  class RecordingScenarioAudit : public loka::dsl::testing::ScenarioAuditSink
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

    std::vector<loka::dsl::testing::ScenarioStepTerminal> steps;
    std::vector<loka::dsl::testing::ScenarioAuditTerminalStatus> terminals;
  };

  class RefusingScenarioAudit : public loka::dsl::testing::ScenarioAuditSink
  {
  public:
    RefusingScenarioAudit()
        : stepCalls(0),
          terminalCalls(0)
    {
    }

    virtual bool recordStep(const loka::dsl::testing::ScenarioStepTerminal &)
    {
      ++this->stepCalls;
      return false;
    }

    virtual bool recordTerminal(loka::dsl::testing::ScenarioAuditTerminalStatus)
    {
      ++this->terminalCalls;
      return false;
    }

    int stepCalls;
    int terminalCalls;
  };

  class ScrapbookTourPlatformContext : public NullPlatformContext
  {
  public:
    explicit ScrapbookTourPlatformContext(const loka::core::String &packagePath)
        : packagePath_(packagePath)
    {
    }

    virtual bool openFile(const loka::file::File &item, loka::platform::file::FileHandle &out) const
    {
      if (item.base() != loka::file::File::BASE_APPLICATION
          || !item.relativePath().equals(loka::core::String::Literal("ASSETS.LRP")))
      {
        return false;
      }
      out = loka::platform::file::FileHandle();
      out.displayPath = this->packagePath_;
      return true;
    }

    virtual bool createImageFromBlob(const loka::core::resource::Blob &blob,
                                     std::size_t offset,
                                     std::size_t length,
                                     loka::core::resource::Image &out) const
    {
      (void)blob;
      (void)offset;
      (void)length;
      out = loka::core::resource::Image::FromNative(reinterpret_cast<void *>(1), 1, 1, 0, 0);
      return out.isValid();
    }

  private:
    loka::core::String packagePath_;
  };

  std::string SourcePath(const char *relative)
  {
    const std::string sourceFile(__FILE__);
    const std::string unixMarker = "/tests/ScrapbookScenarioTests.cpp";
    const std::string windowsMarker = "\\tests\\ScrapbookScenarioTests.cpp";
    std::string::size_type marker = sourceFile.rfind(unixMarker);
    if (marker == std::string::npos)
    {
      marker = sourceFile.rfind(windowsMarker);
    }
    assert(marker != std::string::npos);
    return sourceFile.substr(0, marker) + "/" + (relative ? relative : "");
  }

  void VerifyCurrentPage(const scrapbook::MainNode &mainNode, int expected)
  {
    int actual = -1;
    LOKA_VERIFY(mainNode.queryCurrentPageIndex(actual));
    LOKA_VERIFY(actual == expected);
  }

  void VerifyPlanValidity(const loka::scenario_tests::ScenarioLaunchPlan &plan, bool expected)
  {
    const bool actual = plan.isValid();
    LOKA_VERIFY(actual == expected);
  }

  void VerifyRecordInt(const loka::dsl::SnapRecord &record, const char *key, long expected)
  {
    long actual = -1;
    const bool available = record.getInt(key, actual);
    LOKA_VERIFY(available);
    LOKA_VERIFY(actual == expected);
  }

  loka::scenario_tests::ScenarioAdvance Advance(loka::scenario_tests::ScrapbookScenario &scenario,
                                                long tick,
                                                loka::app::scene::Scene &scene,
                                                scrapbook::MainNode &mainNode,
                                                loka::dsl::SnapRecord &record)
  {
    loka::scenario_tests::CaptureContentBounds bounds;
    bounds.available = true;
    bounds.right = 300;
    bounds.bottom = 170;
    loka::core::StateTrackerGuard guard(mainNode.tracker());
    return scenario.step(tick, &scene, mainNode, bounds, record);
  }
} // namespace

void testScrapbookRigLaunchRequiresConfigAndRefusesStandaloneTour()
{
  loka::dsl::SnapTestConfig::Settings settings;
  const bool loaded = loka::dsl::SnapTestConfig::load("definitely-missing-LokaTest.cfg", settings);
  LOKA_VERIFY(!loaded);

  loka::scenario_tests::ScenarioLaunchPlan plan;
  LOKA_VERIFY(!loka::scenario_tests::QueryRigLaunchPlan(loaded, settings, plan));
  VerifyPlanValidity(plan, false);

  settings.hasScenario = true;
  settings.scenario = "standalone-tour";
  LOKA_VERIFY(!loka::scenario_tests::QueryRigLaunchPlan(true, settings, plan));
  VerifyPlanValidity(plan, false);

  settings.scenario = "startup";
  LOKA_VERIFY(loka::scenario_tests::QueryRigLaunchPlan(true, settings, plan));
  VerifyPlanValidity(plan, true);
  LOKA_VERIFY(plan.scenario() == "startup");
  LOKA_VERIFY(plan.completionPolicy() == loka::scenario_tests::SCENARIO_COMPLETION_DRIVER_OWNED);

  plan = loka::scenario_tests::ScenarioLaunchPlan::StandaloneTour();
  LOKA_VERIFY(!loka::scenario_tests::QueryRigLaunchPlan(false, settings, plan));
  LOKA_VERIFY(plan.scenario() == "standalone-tour");
  LOKA_VERIFY(plan.completionPolicy() == loka::scenario_tests::SCENARIO_COMPLETION_HOLD_FINAL_SCENE);

  std::printf("testScrapbookRigLaunchRequiresConfigAndRefusesStandaloneTour passed\n");
}

void testScrapbookStandaloneTourAdvancesInOrderAndHoldsFinalScene()
{
  const loka::scenario_tests::ScenarioLaunchPlan plan = loka::scenario_tests::ScenarioLaunchPlan::StandaloneTour();
  VerifyPlanValidity(plan, true);
  LOKA_VERIFY(plan.scenario() == "standalone-tour");
  LOKA_VERIFY(plan.completionPolicy() == loka::scenario_tests::SCENARIO_COMPLETION_HOLD_FINAL_SCENE);

  const std::string packagePath = SourcePath("example/ScrapbookUI/assets/ASSETS-modern.LRP");
  ScrapbookTourPlatformContext context(loka::core::String::Utf8(packagePath.data(), packagePath.size()));

  scrapbook::MainProps props;
  props.platformContext(&context);
  loka::app::scene::BoundaryDefinition<scrapbook::MainProps, scrapbook::MainNode> definition(props);
  loka::core::OwnedDef<loka::app::scene::NodeDefinitionBase> root(definition.clone());
  LOKA_VERIFY(root.get() != 0);
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(root.take());
  scene.mount(&platform);
  scene.updateAttached(true);

  scrapbook::MainNode *mainNode =
      static_cast<scrapbook::MainNode *>(loka::dsl::testing::SceneTestAccess::rootNode(scene));
  LOKA_VERIFY(mainNode != 0);
  loka::app::ButtonNode *nextButton = 0;
  loka::dsl::FlowError lookupError;
  LOKA_VERIFY(loka::dsl::testing::LookupNodeById<loka::app::ButtonNode>(
                 &scene, std::string(scrapbook::scene_ids::NextButton()), nextButton, lookupError)
              == loka::dsl::FLOW_STEP_SUCCEEDED);
  LOKA_VERIFY(nextButton != 0);
  VerifyCurrentPage(*mainNode, 0);

  RecordingScenarioAudit audit;
  loka::scenario_tests::ScrapbookScenario scenario(plan, &audit);
  loka::dsl::SnapRecord record;
  LOKA_VERIFY(Advance(scenario, 1, scene, *mainNode, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
  LOKA_VERIFY(audit.steps.empty());
  VerifyCurrentPage(*mainNode, 0);
  LOKA_VERIFY(Advance(scenario, 2, scene, *mainNode, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
  LOKA_VERIFY(audit.steps.size() == 2);
  LOKA_VERIFY(audit.steps[0].stepId() == 1);
  LOKA_VERIFY(audit.steps[0].name() == "verify-page-1");
  LOKA_VERIFY(audit.steps[0].dueTick() == 2);
  LOKA_VERIFY(audit.steps[0].tick() == 2);
  LOKA_VERIFY(audit.steps[0].status() == loka::dsl::FLOW_STEP_SUCCEEDED);
  LOKA_VERIFY(audit.steps[1].stepId() == 2);
  LOKA_VERIFY(audit.steps[1].name() == "advance-to-page-2");
  VerifyCurrentPage(*mainNode, 1);
  LOKA_VERIFY(Advance(scenario, 16, scene, *mainNode, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
  LOKA_VERIFY(audit.steps.size() == 2);
  VerifyCurrentPage(*mainNode, 1);
  LOKA_VERIFY(Advance(scenario, 17, scene, *mainNode, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
  VerifyCurrentPage(*mainNode, 2);
  LOKA_VERIFY(Advance(scenario, 31, scene, *mainNode, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
  VerifyCurrentPage(*mainNode, 2);
  LOKA_VERIFY(Advance(scenario, 32, scene, *mainNode, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
  VerifyCurrentPage(*mainNode, 3);
  LOKA_VERIFY(Advance(scenario, 46, scene, *mainNode, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
  VerifyCurrentPage(*mainNode, 3);
  LOKA_VERIFY(Advance(scenario, 47, scene, *mainNode, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
  VerifyCurrentPage(*mainNode, 4);
  LOKA_VERIFY(Advance(scenario, 61, scene, *mainNode, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
  VerifyCurrentPage(*mainNode, 4);
  LOKA_VERIFY(Advance(scenario, 62, scene, *mainNode, record)
              == loka::scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD);
  LOKA_VERIFY(audit.steps.size() == 12);
  LOKA_VERIFY(audit.steps[11].name() == "capture-final-page");
  LOKA_VERIFY(audit.terminals.size() == 1);
  LOKA_VERIFY(audit.terminals[0] == loka::dsl::testing::SCENARIO_AUDIT_SUCCEEDED);
  VerifyCurrentPage(*mainNode, 4);

  std::string text;
  VerifyRecordInt(record, "final_page", 4);
  VerifyRecordInt(record, "view.target.present", 1);
  LOKA_VERIFY(record.get("text_matches_package_asset", text) && text == "true");
  LOKA_VERIFY(record.get("status", text) && text == loka::dsl::SnapStatusOk());

  LOKA_VERIFY(Advance(scenario, 63, scene, *mainNode, record)
              == loka::scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD);
  LOKA_VERIFY(audit.steps.size() == 12);
  LOKA_VERIFY(audit.terminals.size() == 1);
  VerifyCurrentPage(*mainNode, 4);

  mainNode->selectPage(0);
  RecordingScenarioAudit failedAudit;
  loka::scenario_tests::ScrapbookScenario failedScenario(plan, &failedAudit);
  LOKA_VERIFY(Advance(failedScenario, 2, scene, *mainNode, record)
              == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
  LOKA_VERIFY(failedAudit.steps.size() == 2);
  mainNode->selectPage(0);
  LOKA_VERIFY(Advance(failedScenario, 17, scene, *mainNode, record)
              == loka::scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD);
  LOKA_VERIFY(failedAudit.steps.size() == 3);
  LOKA_VERIFY(failedAudit.steps[2].name() == "verify-page-2");
  LOKA_VERIFY(failedAudit.steps[2].status() == loka::dsl::FLOW_STEP_FAILED);
  LOKA_VERIFY(failedAudit.terminals.size() == 1);
  LOKA_VERIFY(failedAudit.terminals[0] == loka::dsl::testing::SCENARIO_AUDIT_FAILED);
  LOKA_VERIFY(Advance(failedScenario, 18, scene, *mainNode, record)
              == loka::scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD);
  LOKA_VERIFY(failedAudit.steps.size() == 3);
  LOKA_VERIFY(failedAudit.terminals.size() == 1);

  RecordingScenarioAudit canceledAudit;
  loka::scenario_tests::ScrapbookScenario canceledScenario(plan, &canceledAudit);
  canceledScenario.stop();
  canceledScenario.stop();
  LOKA_VERIFY(canceledAudit.steps.empty());
  LOKA_VERIFY(canceledAudit.terminals.size() == 1);
  LOKA_VERIFY(canceledAudit.terminals[0] == loka::dsl::testing::SCENARIO_AUDIT_CANCELED);
  LOKA_VERIFY(Advance(canceledScenario, 62, scene, *mainNode, record)
              == loka::scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD);
  LOKA_VERIFY(canceledAudit.steps.empty());
  LOKA_VERIFY(canceledAudit.terminals.size() == 1);

  mainNode->selectPage(0);
  RefusingScenarioAudit refusingAudit;
  loka::scenario_tests::ScrapbookScenario refusedScenario(plan, &refusingAudit);
  LOKA_VERIFY(Advance(refusedScenario, 2, scene, *mainNode, record)
              == loka::scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD);
  VerifyRecordInt(record, "error_code", 2306);
  LOKA_VERIFY(refusingAudit.stepCalls == 1);
  LOKA_VERIFY(refusingAudit.terminalCalls == 1);
  LOKA_VERIFY(Advance(refusedScenario, 3, scene, *mainNode, record)
              == loka::scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD);
  LOKA_VERIFY(refusingAudit.stepCalls == 1);
  LOKA_VERIFY(refusingAudit.terminalCalls == 1);

  std::printf("testScrapbookStandaloneTourAdvancesInOrderAndHoldsFinalScene passed\n");
}

void testScenarioAuditFileWritesReadableRecords()
{
  const char *path = "_loka_scenario_audit_test.txt";
  std::remove(path);
  {
    loka::platform::file::FileHandle destination;
    destination.displayPath = loka::core::String::Literal(path);
    loka::dsl::testing::ScenarioAuditFile audit(destination, "standalone-tour");
    const bool auditIsValid = audit.isValid();
    LOKA_VERIFY(auditIsValid);
    loka::dsl::FlowError error;
    const loka::dsl::testing::ScenarioStepTerminal step(
        3, "verify page\t2", 17, 18, loka::dsl::FLOW_STEP_FAILED, error);
    LOKA_VERIFY(audit.recordStep(step));
    LOKA_VERIFY(audit.recordTerminal(loka::dsl::testing::SCENARIO_AUDIT_FAILED));
  }

  FILE *input = std::fopen(path, "rb");
  LOKA_VERIFY(input != 0);
  std::string content;
  char buffer[128];
  std::size_t count = 0;
  while ((count = std::fread(buffer, 1, sizeof(buffer), input)) != 0)
  {
    content.append(buffer, count);
  }
  LOKA_VERIFY(std::fclose(input) == 0);
  std::remove(path);

  LOKA_VERIFY(content == "loka_scenario_audit version=1 scenario=standalone-tour\n"
                         "step id=3 due_tick=17 tick=18 status=failed error_kind=0 error_code=0 name=verify%20page%092\n"
                         "terminal status=failed\n");

  std::printf("testScenarioAuditFileWritesReadableRecords passed\n");
}
