#include "HelloWorldStandaloneFlowAppConfig.hpp"

#include "../scenarios/ScenarioWindow.hpp"
#include "StandaloneScenarioSupport.hpp"
#include "app/core/App.hpp"
#include "app/core/Window.hpp"

namespace loka
{
  namespace standalone_tests
  {
    HelloWorldStandaloneFlowAppConfig::HelloWorldStandaloneFlowAppConfig(PlatformContext *context,
                                                                         const platform::file::FileHandle *auditFile,
                                                                         std::FILE *diagnostics)
        : MyAppConfig(context),
          audit_(auditFile ? *auditFile : ResolveStandaloneAuditFile(), "toggle-action-probe"),
          scenario_(scenario_tests::SCENARIO_COMPLETION_HOLD_FINAL_SCENE, &this->audit_),
          borrowedMainNode_(0),
          mountDeadline_("HelloWorld", diagnostics)
    {
    }

    HelloWorldStandaloneFlowAppConfig::~HelloWorldStandaloneFlowAppConfig()
    {
      this->scenario_.stop();
    }

    int HelloWorldStandaloneFlowAppConfig::exitCode() const
    {
      return this->audit_.isValid() && !this->mountDeadline_.failed() ? 0 : 1;
    }

    void HelloWorldStandaloneFlowAppConfig::setApp(App *app)
    {
      this->mountDeadline_.setApp(app);
    }

    void HelloWorldStandaloneFlowAppConfig::compose(AppComposition &composition)
    {
      composition << scenario_tests::MakeScenarioWindow<helloworld::MainProps, helloworld::MainNode>(
          helloworld::MainProps(),
          &this->borrowedMainNode_,
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
      const StandaloneMountDeadline::Advance mountAdvance = this->mountDeadline_.advance(this->borrowedMainNode_ != 0);
      switch (mountAdvance)
      {
      case StandaloneMountDeadline::ADVANCE_WAITING:
      case StandaloneMountDeadline::ADVANCE_FAILED:
        return;
      case StandaloneMountDeadline::ADVANCE_MOUNTED:
        break;
      }
      if (!window || !window->scene())
      {
        return;
      }
      dsl::SnapRecord record;
      const scenario_tests::ScenarioAdvance advance =
          this->scenario_.step(this->mountDeadline_.tick(), window->scene(), StandaloneContentBounds(window), record);
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
