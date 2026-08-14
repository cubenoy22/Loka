#include "HelloWorldStandaloneFlowApplication.hpp"

#include <cassert>

#include "HelloWorldScenarios.hpp"
#include "MainNode.hpp"
#include "ScenarioWindow.hpp"
#include "StandaloneScenarioSupport.hpp"
#include "app/bootstrap/PlatformBootstrap.hpp"
#include "app/core/App.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/core/Window.hpp"
#include "core/util/ScopedPtr.hpp"
#include "testing/scene/ScenarioAudit.hpp"

namespace loka
{
  namespace standalone_tests
  {
    namespace
    {
      class HelloWorldStandaloneFlowAppConfig : public AppConfigurable
      {
      public:
        explicit HelloWorldStandaloneFlowAppConfig(PlatformContext *context)
            : AppConfigurable(context),
              audit_(ResolveStandaloneAuditFile(), "toggle-action-probe"),
              scenario_(scenario_tests::SCENARIO_COMPLETION_HOLD_FINAL_SCENE, &this->audit_),
              tick_(0)
        {
        }

        virtual ~HelloWorldStandaloneFlowAppConfig()
        {
          this->scenario_.stop();
        }

        bool isValid() const
        {
          return this->audit_.isValid();
        }

        virtual void compose(AppComposition &composition)
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

      private:
        static void OnWindowIdle(Window *window, double elapsedSeconds, void *userData)
        {
          (void)elapsedSeconds;
          HelloWorldStandaloneFlowAppConfig *self = static_cast<HelloWorldStandaloneFlowAppConfig *>(userData);
          if (self)
          {
            self->tick(window);
          }
        }

        void tick(Window *window)
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

        dsl::testing::ScenarioAuditFile audit_;
        scenario_tests::HelloWorldScenario scenario_;
        long tick_;
      };
    } // namespace

    int RunHelloWorldStandaloneFlowApplication(HINSTANCE hInstance, int nCmdShow)
    {
      platform::InitPlatformRuntime();
      core::ScopedPtr<PlatformContext> platformContext(platform::CreatePlatformContext());
      assert(platformContext.get() && "PlatformContext is required");
      if (!platformContext.get())
      {
        return 1;
      }
      HelloWorldStandaloneFlowAppConfig config(platformContext.get());
      if (!config.isValid())
      {
        return 1;
      }
      core::ScopedPtr<App> app(platformContext->createApp(&config, hInstance, nCmdShow));
      assert(app.get() && "App is required");
      if (!app.get())
      {
        return 1;
      }
      app->run();
      return 0;
    }
  } // namespace standalone_tests
} // namespace loka
