#include "HelloWorldScenarioTests.hpp"

#include "support/TestVerify.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "../example/HelloWorld/src/MainNode.hpp"
#include "app/scene/Scene.hpp"
#include "core/util/OwnedDef.hpp"
#include "scenarios/HelloWorldScenarios.hpp"
#include "standalone/HelloWorldStandaloneFlowAppConfig.hpp"
#include "support/MenuPresentationVerify.hpp"
#include "testing/scene/ScenarioAudit.hpp"

namespace
{
  class HelloWorldScenarioPlatform : public loka::app::scene::IPlatformController
  {
  public:
    virtual void onChange(loka::app::scene::Node *, loka::app::scene::NodeDirtyFlags, bool) {}

    virtual void synchronize() {}

    virtual bool hasPendingSync() const
    {
      return false;
    }

    virtual void destroy() {}
  };

  class RecordingHelloWorldAudit : public loka::dsl::testing::ScenarioAuditSink
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

  void VerifyRecordInt(const loka::dsl::SnapRecord &record, const char *key, long expected)
  {
    long actual = -1;
    const bool available = record.getInt(key, actual);
    LOKA_VERIFY(available);
    LOKA_VERIFY(actual == expected);
  }
} // namespace

void testHelloWorldToggleActionProbeDrivesOwnerCommands()
{
  helloworld::MainProps props;
  loka::app::scene::BoundaryDefinition<helloworld::MainProps, helloworld::MainNode> definition(props);
  loka::core::OwnedDef<loka::app::scene::NodeDefinitionBase> root(definition.clone());
  LOKA_VERIFY(root.get() != 0);
  HelloWorldScenarioPlatform platform;
  loka::app::scene::Scene scene(root.take());
  scene.mount(&platform);
  scene.updateAttached(true);

  RecordingHelloWorldAudit audit;
  loka::scenario_tests::HelloWorldScenario scenario(loka::scenario_tests::SCENARIO_COMPLETION_DRIVER_OWNED, &audit);
  loka::scenario_tests::CaptureContentBounds bounds;
  bounds.available = true;
  bounds.right = 420;
  bounds.bottom = 300;
  loka::dsl::SnapRecord record;

  LOKA_VERIFY(scenario.step(1, &scene, bounds, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
  LOKA_VERIFY(scenario.step(2, &scene, bounds, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
  LOKA_VERIFY(scenario.step(32, &scene, bounds, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
  LOKA_VERIFY(scenario.step(62, &scene, bounds, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
  LOKA_VERIFY(scenario.step(92, &scene, bounds, record)
              == loka::scenario_tests::SCENARIO_ADVANCE_DRIVER_COMPLETION_READY);
  LOKA_VERIFY(scenario.step(93, &scene, bounds, record)
              == loka::scenario_tests::SCENARIO_ADVANCE_DRIVER_COMPLETION_READY);

  std::string value;
  LOKA_VERIFY(record.get("test", value) && value == "HelloWorld");
  LOKA_VERIFY(record.get("step", value) && value == "toggle-action-probe");
  LOKA_VERIFY(record.get("node", value) && value == "HelloWorld.LeftPanel.ActionSummary");
  LOKA_VERIFY(record.get("text.value", value) && value == "Button enabled: no / clicks: 1");
  LOKA_VERIFY(record.get("disabled_probe_ignored", value) && value == "true");
  VerifyRecordInt(record, "tick", 92);
  VerifyRecordInt(record, "crop_left", 0);
  VerifyRecordInt(record, "crop_top", 0);
  VerifyRecordInt(record, "crop_right", 420);
  VerifyRecordInt(record, "crop_bottom", 300);
  LOKA_VERIFY(audit.steps.size() == 8);
  LOKA_VERIFY(audit.steps[1].name() == "probe-enabled-action");
  LOKA_VERIFY(audit.steps[3].name() == "toggle-probe-disabled");
  LOKA_VERIFY(audit.steps[5].name() == "probe-disabled-action");
  LOKA_VERIFY(audit.terminals.size() == 1);
  LOKA_VERIFY(audit.terminals[0] == loka::dsl::testing::SCENARIO_AUDIT_SUCCEEDED);

  scene.unmount();
  std::printf("testHelloWorldToggleActionProbeDrivesOwnerCommands passed\n");
}

void testHelloWorldToggleActionProbeHoldsFinalScene()
{
  helloworld::MainProps props;
  loka::app::scene::BoundaryDefinition<helloworld::MainProps, helloworld::MainNode> definition(props);
  loka::core::OwnedDef<loka::app::scene::NodeDefinitionBase> root(definition.clone());
  LOKA_VERIFY(root.get() != 0);
  HelloWorldScenarioPlatform platform;
  loka::app::scene::Scene scene(root.take());
  scene.mount(&platform);
  scene.updateAttached(true);

  RecordingHelloWorldAudit audit;
  loka::scenario_tests::HelloWorldScenario scenario(loka::scenario_tests::SCENARIO_COMPLETION_HOLD_FINAL_SCENE, &audit);
  loka::scenario_tests::CaptureContentBounds bounds;
  bounds.available = true;
  bounds.right = 420;
  bounds.bottom = 300;
  loka::dsl::SnapRecord record;

  LOKA_VERIFY(scenario.step(2, &scene, bounds, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
  LOKA_VERIFY(scenario.step(32, &scene, bounds, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
  LOKA_VERIFY(scenario.step(62, &scene, bounds, record) == loka::scenario_tests::SCENARIO_ADVANCE_PENDING);
  LOKA_VERIFY(scenario.step(92, &scene, bounds, record) == loka::scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD);
  LOKA_VERIFY(scenario.step(93, &scene, bounds, record) == loka::scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD);

  std::string value;
  LOKA_VERIFY(record.get("text.value", value) && value == "Button enabled: no / clicks: 1");
  LOKA_VERIFY(audit.terminals.size() == 1);
  LOKA_VERIFY(audit.terminals[0] == loka::dsl::testing::SCENARIO_AUDIT_SUCCEEDED);

  scene.unmount();
  std::printf("testHelloWorldToggleActionProbeHoldsFinalScene passed\n");
}

void testHelloWorldStandaloneMenuMatchesExample()
{
  MyAppConfig example(0);
  loka::standalone_tests::HelloWorldStandaloneFlowAppConfig standalone(0);
  loka::app::MenuBarDefinition exampleMenu;
  loka::app::MenuBarDefinition standaloneMenu;

  std::srand(1);
  loka::testing::ComposeMenuBar(example, exampleMenu);
  std::srand(1);
  loka::testing::ComposeMenuBar(standalone, standaloneMenu);

  LOKA_VERIFY(!exampleMenu.empty());
  LOKA_VERIFY(loka::testing::MenuPresentationsEqual(exampleMenu, standaloneMenu));

  std::printf("testHelloWorldStandaloneMenuMatchesExample passed\n");
}
