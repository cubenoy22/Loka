#include "SmirkyCardScenarioTests.hpp"

#include "support/TestVerify.hpp"
#include "support/WindowAdmissionTestApp.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "platform/null/NullWindow.hpp"
#include "scenarios/SmirkyCardScenarios.hpp"
#include "scenarios/ScenarioReel.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "MyAppConfig.hpp"

#include <cstdio>

void testSmirkyCardScenarioCellsUseAppAdmission()
{
  NullPlatformContext context;
  SmirkyCardAppConfig config(&context);
  AppComposition composition(&context);
  config.compose(composition);
  std::vector<AppComponent *> components = composition.build();
  LOKA_VERIFY(components.size() == 1);
  Window *window = components[0]->asWindow();
  LOKA_VERIFY(window != 0 && window->scene() != 0);
  WindowAdmissionTestApp admission(*window);
  loka::dsl::testing::SceneTestAccess::updateAttached(*window->scene(), true);

  loka::scenario_tests::ScenarioReel<loka::scenario_tests::SmirkyCardScenario> reel(
      loka::scenario_tests::SmirkyCardReelCells(),
      loka::scenario_tests::STARTUP_EXAMPLE_HELLO_WORLD,
      &loka::scenario_tests::MakeSmirkyCardDriverErrorRecord, 2904, 0.0, 1);
  for (int tick = 0; tick < 32 && !reel.finished(); ++tick)
  {
    // This is #668's App admission seam, the same seam production consumes;
    // the scenario itself never replaces a Scene or calls SceneTestAccess.
    reel.tick(window, &admission, 0.1, window->getTracker());
    admission.flush();
  }
  LOKA_VERIFY(reel.finished());
  LOKA_VERIFY(reel.completedCycles() == 1);
  loka::app::scene::Node *titleNode = loka::dsl::testing::SceneTestAccess::rootNode(*window->scene());
  (void)titleNode;
  loka::app::TextNode *title = 0;
  loka::dsl::FlowError error;
  LOKA_VERIFY(loka::dsl::testing::LookupNodeById<loka::app::TextNode>(
      window->scene(), "SmirkyCard.Title", title, error) == loka::dsl::FLOW_STEP_SUCCEEDED);
  LOKA_VERIFY(title->props.text_->get().compare(loka::core::String::Literal("Card Two")) == 0);
  delete window;
  std::printf("testSmirkyCardScenarioCellsUseAppAdmission passed\n");
}
