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
        : HelloWorldAppConfig(context, 0x13579BDFUL),
          audit_(auditFile ? *auditFile : ResolveStandaloneAuditFile(), "toggle-action-probe"),
          scenario_(scenario_tests::SCENARIO_COMPLETION_HOLD_FINAL_SCENE, &this->audit_),
          borrowedMainNode_(0),
          runControl_("HelloWorld", diagnostics)
    {
    }

    HelloWorldStandaloneFlowAppConfig::~HelloWorldStandaloneFlowAppConfig()
    {
      this->scenario_.stop();
    }

    int HelloWorldStandaloneFlowAppConfig::exitCode() const
    {
      return this->audit_.isValid() && !this->runControl_.failed() ? 0 : 1;
    }

    void HelloWorldStandaloneFlowAppConfig::setApp(App *app)
    {
      this->runControl_.setApp(app);
    }

    void HelloWorldStandaloneFlowAppConfig::compose(AppComposition &composition)
    {
      composition << scenario_tests::MakeScenarioWindow<helloworld::MainProps, helloworld::MainNode>(
          helloworld::MainProps(),
          &this->borrowedMainNode_,
          420,
          330,
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
      const StandaloneRunControl::Advance mountAdvance = this->runControl_.advance(this->borrowedMainNode_ != 0);
      switch (mountAdvance)
      {
      case StandaloneRunControl::ADVANCE_WAITING:
      case StandaloneRunControl::ADVANCE_FAILED:
        return;
      case StandaloneRunControl::ADVANCE_MOUNTED:
        break;
      }
      if (!window || !window->scene())
      {
        return;
      }
      dsl::SnapRecord record;
      const scenario_tests::ScenarioAdvance advance =
          this->scenario_.step(this->runControl_.tick(), window->scene(), StandaloneContentBounds(window), record);
      this->runControl_.observeScenarioAdvance(advance, record);
    }
  } // namespace standalone_tests
} // namespace loka
