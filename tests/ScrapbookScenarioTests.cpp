#include "ScrapbookScenarioTests.hpp"

#include "support/TestVerify.hpp"

#include <cassert>
#include <cstdio>
#include <string>

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

namespace
{
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
                                                scrapbook::MainNode &mainNode,
                                                loka::dsl::SnapRecord &record)
  {
    loka::scenario_tests::CaptureContentBounds bounds;
    bounds.available = true;
    bounds.right = 300;
    bounds.bottom = 170;
    loka::core::StateTrackerGuard guard(mainNode.tracker());
    return scenario.step(tick, mainNode, bounds, record);
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
  VerifyCurrentPage(*mainNode, 0);

  loka::scenario_tests::ScrapbookScenario scenario(plan);
  loka::dsl::SnapRecord record;
  LOKA_VERIFY(Advance(scenario, 1, *mainNode, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
  VerifyCurrentPage(*mainNode, 0);
  LOKA_VERIFY(Advance(scenario, 2, *mainNode, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
  VerifyCurrentPage(*mainNode, 1);
  LOKA_VERIFY(Advance(scenario, 31, *mainNode, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
  VerifyCurrentPage(*mainNode, 1);
  LOKA_VERIFY(Advance(scenario, 32, *mainNode, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
  VerifyCurrentPage(*mainNode, 4);
  LOKA_VERIFY(Advance(scenario, 61, *mainNode, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
  VerifyCurrentPage(*mainNode, 4);
  LOKA_VERIFY(Advance(scenario, 62, *mainNode, record) == loka::scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD);
  VerifyCurrentPage(*mainNode, 4);

  std::string text;
  VerifyRecordInt(record, "step1_page", 0);
  VerifyRecordInt(record, "step2_page", 1);
  VerifyRecordInt(record, "final_page", 4);
  LOKA_VERIFY(record.get("text_matches_package_asset", text) && text == "true");
  LOKA_VERIFY(record.get("status", text) && text == loka::dsl::SnapStatusOk());

  LOKA_VERIFY(Advance(scenario, 63, *mainNode, record) == loka::scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD);
  VerifyCurrentPage(*mainNode, 4);

  std::printf("testScrapbookStandaloneTourAdvancesInOrderAndHoldsFinalScene passed\n");
}
