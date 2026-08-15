#include "HelloWorldScenarioDriver.hpp"

#include <cassert>

#include "HelloWorldScenarios.hpp"
#include "MainNode.hpp"
#include "ScenarioDriverSupport.hpp"
#include "ScenarioWindow.hpp"
#include "app/PlatformContext.hpp"
#include "app/bootstrap/PlatformBootstrap.hpp"
#include "app/core/App.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "core/util/ScopedPtr.hpp"

namespace loka
{
  namespace toolbox_tests
  {
    namespace
    {
      const char *kConfigPath = "LokaTest.cfg";

      class HelloWorldScenarioAppConfig : public AppConfigurable
      {
      public:
        HelloWorldScenarioAppConfig(PlatformContext *context, const dsl::SnapTestConfig::Settings &settings)
            : AppConfigurable(context),
              settings_(settings),
              scenario_(scenario_tests::SCENARIO_COMPLETION_DRIVER_OWNED),
              borrowedApp_(0),
              recorded_(false),
              tickCount_(0),
              lingerRemaining_(settings.hasLingerSeconds ? static_cast<double>(settings.lingerSeconds) : 0.0),
              hostCompletionSignal_()
        {
        }

        virtual ~HelloWorldScenarioAppConfig()
        {
          this->scenario_.stop();
        }

        void setApp(App *app)
        {
          this->borrowedApp_ = app;
        }

        virtual void compose(AppComposition &composition)
        {
          composition << scenario_tests::MakeScenarioWindow<helloworld::MainProps, helloworld::MainNode>(
              helloworld::MainProps(),
              0,
              420,
              300,
              "LokaHelloWorldTestsToolbox",
              app::IdlePolicy::everyTick(),
              &HelloWorldScenarioAppConfig::OnWindowIdle,
              this);
        }

      private:
        static void OnWindowIdle(Window *window, double elapsedSeconds, void *userData)
        {
          HelloWorldScenarioAppConfig *self = static_cast<HelloWorldScenarioAppConfig *>(userData);
          if (self)
          {
            self->tick(window, elapsedSeconds);
          }
        }

        void tick(Window *window, double elapsedSeconds)
        {
          ++this->tickCount_;
          if (!this->recorded_)
          {
            dsl::SnapRecord record;
            bool done = false;
            if (!window || !window->scene())
            {
              record = scenario_tests::MakeHelloWorldDriverErrorRecord(2402, "Scene was not mounted");
              done = true;
            }
            else
            {
              const scenario_tests::CaptureContentBounds captureBounds = QueryCaptureContentBounds(window);
              const scenario_tests::ScenarioAdvance advance =
                  this->scenario_.step(this->tickCount_, window->scene(), ContentLocalBounds(captureBounds), record);
              switch (advance)
              {
              case scenario_tests::SCENARIO_ADVANCE_PENDING:
                break;
              case scenario_tests::SCENARIO_ADVANCE_DRIVER_COMPLETION_READY:
                done = true;
                (void)WriteCaptureMetadata(this->settings_,
                                           "HelloWorld.capture.toolbox",
                                           this->scenario_.name().c_str(),
                                           this->tickCount_,
                                           captureBounds);
                break;
              case scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD:
                return;
              }
            }
            if (done)
            {
              (void)WriteScenarioRecord(this->settings_, record);
              this->recorded_ = true;
              (void)this->hostCompletionSignal_.publish();
            }
          }
          if (!this->recorded_)
          {
            return;
          }
          this->lingerRemaining_ -= elapsedSeconds;
          if (this->lingerRemaining_ <= 0.0 && this->borrowedApp_)
          {
            this->borrowedApp_->quit();
          }
        }

        const dsl::SnapTestConfig::Settings settings_;
        scenario_tests::HelloWorldScenario scenario_;
        App *borrowedApp_;
        bool recorded_;
        long tickCount_;
        double lingerRemaining_;
        HostCompletionSignal hostCompletionSignal_;
      };
    } // namespace

    int RunHelloWorldScenarioApplication()
    {
      dsl::SnapTestConfig::Settings settings;
      const bool configLoaded = dsl::SnapTestConfig::load(kConfigPath, settings);
      if (!configLoaded)
      {
        (void)WriteScenarioRecord(
            settings, scenario_tests::MakeHelloWorldDriverErrorRecord(2400, "LokaTest.cfg is missing or invalid"));
        return 0;
      }
      if (!settings.hasScenario || !scenario_tests::IsHelloWorldScenario(settings.scenario))
      {
        (void)WriteScenarioRecord(
            settings, scenario_tests::MakeHelloWorldDriverErrorRecord(2401, "scenario is missing or not registered"));
        return 0;
      }

      platform::InitPlatformRuntime();
      core::ScopedPtr<PlatformContext> platformContext(platform::CreatePlatformContext());
      assert(platformContext.get() && "PlatformContext is required");
      if (!platformContext.get())
      {
        return 1;
      }
      HelloWorldScenarioAppConfig config(platformContext.get(), settings);
      core::ScopedPtr<App> app(platformContext->createApp(&config, 0, 0));
      assert(app.get() && "App is required");
      if (!app.get())
      {
        return 1;
      }
      config.setApp(app.get());
      app->run();
      return 0;
    }
  } // namespace toolbox_tests
} // namespace loka
