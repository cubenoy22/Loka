#include "HelloWorldScenarioDriver.hpp"

#include <cassert>

#include "HelloWorldScenarios.hpp"
#include "HelloWorldClassicScenarioPresentation.hpp"
#include "ScenarioDriverSupport.hpp"
#include "StartupScenarios.hpp"
#include "app/PlatformContext.hpp"
#include "app/bootstrap/PlatformBootstrap.hpp"
#include "app/core/App.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "core/util/ScopedPtr.hpp"
#include "testing/scene/ScenarioAudit.hpp"

namespace loka
{
  namespace toolbox_tests
  {
    namespace
    {
      const char *kConfigPath = "LokaTest.cfg";
      const char *kDefaultScenarioName = "toggle-action-probe";

      class HelloWorldScenarioAppConfig : public scenario_tests::HelloWorldClassicScenarioPresentation
      {
      public:
        HelloWorldScenarioAppConfig(PlatformContext *context, const dsl::SnapTestConfig::Settings &settings)
            : scenario_tests::HelloWorldClassicScenarioPresentation(context),
              startup_(scenario_tests::IsStartupScenario(settings.scenario)),
              audit_(ResolveScenarioAuditFile(), settings.scenario.c_str()),
              startupScenario_(scenario_tests::STARTUP_EXAMPLE_HELLO_WORLD,
                               scenario_tests::SCENARIO_COMPLETION_DRIVER_OWNED,
                               &this->audit_),
              scenario_(scenario_tests::SCENARIO_COMPLETION_DRIVER_OWNED, &this->audit_),
              borrowedApp_(0),
              recorded_(false),
              tickCount_(0),
              lingerRemaining_(settings.hasLingerSeconds ? static_cast<double>(settings.lingerSeconds) : 0.0),
              hostCompletionSignal_()
        {
        }

        virtual ~HelloWorldScenarioAppConfig()
        {
          if (this->startup_)
          {
            this->startupScenario_.stop();
          }
          else
          {
            this->scenario_.stop();
          }
        }

        void setApp(App *app)
        {
          this->borrowedApp_ = app;
        }

      private:
        virtual void onScenarioIdle(Window *window, double elapsedSeconds)
        {
          this->tick(window, elapsedSeconds);
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
              record = this->startup_
                           ? scenario_tests::MakeStartupDriverErrorRecord(
                                 scenario_tests::STARTUP_EXAMPLE_HELLO_WORLD, 2802, "Scene was not mounted")
                           : scenario_tests::MakeHelloWorldDriverErrorRecord(2402, "Scene was not mounted");
              done = true;
            }
            else
            {
              const scenario_tests::CaptureContentBounds captureBounds = QueryCaptureContentBounds(window);
              const scenario_tests::ScenarioAdvance advance =
                  this->startup_
                      ? this->startupScenario_.step(
                            this->tickCount_, window->scene(), ContentLocalBounds(captureBounds), record)
                      : this->scenario_.step(
                            this->tickCount_, window->scene(), ContentLocalBounds(captureBounds), record);
              switch (advance)
              {
              case scenario_tests::SCENARIO_ADVANCE_PENDING:
                break;
              case scenario_tests::SCENARIO_ADVANCE_DRIVER_COMPLETION_READY:
                done = true;
                break;
              case scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD:
                return;
              }
            }
            if (done)
            {
              if (this->startup_)
              {
                (void)this->startupScenario_.publishVerdict(record);
              }
              else
              {
                (void)this->scenario_.publishVerdict(record);
              }
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

        const bool startup_;
        dsl::testing::ScenarioAuditFile audit_;
        scenario_tests::StartupScenario startupScenario_;
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
        (void)WriteScenarioErrorAudit(
            kDefaultScenarioName,
            scenario_tests::MakeHelloWorldDriverErrorRecord(2400, "LokaTest.cfg is missing or invalid"));
        return 0;
      }
      if (!settings.hasScenario
          || (!scenario_tests::IsStartupScenario(settings.scenario)
              && !scenario_tests::IsHelloWorldScenario(settings.scenario)))
      {
        (void)WriteScenarioErrorAudit(
            settings.hasScenario ? settings.scenario.c_str() : kDefaultScenarioName,
            scenario_tests::MakeHelloWorldDriverErrorRecord(2401, "scenario is missing or not registered"));
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
