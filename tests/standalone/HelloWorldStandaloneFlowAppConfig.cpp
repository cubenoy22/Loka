#include "HelloWorldStandaloneFlowAppConfig.hpp"

#include "../scenarios/ScenarioWindow.hpp"
#include "StandaloneScenarioSupport.hpp"
#include "app/core/Window.hpp"

namespace loka
{
  namespace standalone_tests
  {
    HelloWorldStandaloneFlowAppConfig::HelloWorldStandaloneFlowAppConfig(PlatformContext *context)
        : MyAppConfig(context),
          audit_(ResolveStandaloneAuditFile(), "toggle-action-probe"),
          scenario_(scenario_tests::SCENARIO_COMPLETION_HOLD_FINAL_SCENE, &this->audit_),
          tick_(0)
    {
    }

    HelloWorldStandaloneFlowAppConfig::~HelloWorldStandaloneFlowAppConfig()
    {
      this->scenario_.stop();
    }

    bool HelloWorldStandaloneFlowAppConfig::isValid() const
    {
      return this->audit_.isValid();
    }

    void HelloWorldStandaloneFlowAppConfig::compose(AppComposition &composition)
    {
      composition << scenario_tests::MakeScenarioWindow<helloworld::MainProps, helloworld::MainNode>(
          helloworld::MainProps(),
          0,
          420,
          300,
          "Loka HelloWorld Standalone Flow",
          app::IdlePolicy::interval(0.1),
          &HelloWorldStandaloneFlowAppConfig::OnWindowIdle,
          this);
    }

    void HelloWorldStandaloneFlowAppConfig::OnWindowIdle(Window *window, double elapsedSeconds, void *userData)
    {
      (void)elapsedSeconds;
      HelloWorldStandaloneFlowAppConfig *self = static_cast<HelloWorldStandaloneFlowAppConfig *>(userData);
      if (self)
      {
        self->tick(window);
      }
    }

    void HelloWorldStandaloneFlowAppConfig::tick(Window *window)
    {
      ++this->tick_;
      if (!window || !window->scene())
      {
        return;
      }
      dsl::SnapRecord record;
      const scenario_tests::ScenarioAdvance advance =
          this->scenario_.step(this->tick_, window->scene(), StandaloneContentBounds(window), record);
      switch (advance)
      {
      case scenario_tests::SCENARIO_ADVANCE_PENDING:
      case scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD:
      case scenario_tests::SCENARIO_ADVANCE_DRIVER_COMPLETION_READY:
        return;
      }
    }
  } // namespace standalone_tests
} // namespace loka
