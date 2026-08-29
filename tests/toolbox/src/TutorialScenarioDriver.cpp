#include "TutorialScenarioDriver.hpp"

#include <cassert>

#include "ScenarioDriverSupport.hpp"
#include "StartupScenarios.hpp"
#include "TutorialScenarioPresentation.hpp"
#include "TutorialScenarios.hpp"
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
      const char *kDefaultScenarioName = "increment-summary-toggle";

      class TutorialScenarioAppConfig : public scenario_tests::TutorialScenarioPresentation
      {
      public:
        TutorialScenarioAppConfig(PlatformContext *context, const dsl::SnapTestConfig::Settings &settings)
            : scenario_tests::TutorialScenarioPresentation(context,
                                                           scenario_tests::IsStartupScenario(settings.scenario)),
              audit_(ResolveScenarioAuditFile(), settings.scenario.c_str()),
              startupScenario_(scenario_tests::STARTUP_EXAMPLE_TUTORIAL,
                               scenario_tests::SCENARIO_COMPLETION_DRIVER_OWNED,
                               &this->audit_),
              scenario_(scenario_tests::SCENARIO_COMPLETION_DRIVER_OWNED, &this->audit_),
              borrowedApp_(0),
              recorded_(false),
              tickCount_(0),
              lingerRemaining_(settings.hasLingerSeconds ? static_cast<double>(settings.lingerSeconds) : 0.0),
              completionPublisher_()
        {
        }

        virtual ~TutorialScenarioAppConfig()
        {
          if (this->isStartupPresentation())
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
              record = this->isStartupPresentation()
                           ? scenario_tests::MakeStartupDriverErrorRecord(
                                 scenario_tests::STARTUP_EXAMPLE_TUTORIAL, 2802, "Scene was not mounted")
                           : scenario_tests::MakeTutorialDriverErrorRecord(2502, "Scene was not mounted");
              done = true;
            }
            else
            {
              const scenario_tests::CaptureContentBounds captureBounds = QueryCaptureContentBounds(window);
              const scenario_tests::ScenarioAdvance advance =
                  this->isStartupPresentation()
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
              if (this->isStartupPresentation())
              {
                (void)this->startupScenario_.publishVerdict(record);
              }
              else
              {
                (void)this->scenario_.publishVerdict(record);
              }
              this->recorded_ = true;
              (void)this->completionPublisher_.publish(window);
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

        dsl::testing::ScenarioAuditFile audit_;
        scenario_tests::StartupScenario startupScenario_;
        scenario_tests::TutorialScenario scenario_;
        App *borrowedApp_;
        bool recorded_;
        long tickCount_;
        double lingerRemaining_;
        ScenarioCompletionPublisher completionPublisher_;
      };
    } // namespace

    int RunTutorialScenarioApplication()
    {
      dsl::SnapTestConfig::Settings settings;
      const bool configLoaded = dsl::SnapTestConfig::load(kConfigPath, settings);
      if (!configLoaded)
      {
        (void)WriteScenarioErrorAudit(
            kDefaultScenarioName,
            scenario_tests::MakeTutorialDriverErrorRecord(2500, "LokaTest.cfg is missing or invalid"));
        return 0;
      }
      if (!settings.hasScenario
          || (!scenario_tests::IsStartupScenario(settings.scenario)
              && !scenario_tests::IsTutorialScenario(settings.scenario)))
      {
        (void)WriteScenarioErrorAudit(
            settings.hasScenario ? settings.scenario.c_str() : kDefaultScenarioName,
            scenario_tests::MakeTutorialDriverErrorRecord(2501, "scenario is missing or not registered"));
        return 0;
      }

      platform::InitPlatformRuntime();
      core::ScopedPtr<PlatformContext> platformContext(platform::CreatePlatformContext());
      assert(platformContext.get() && "PlatformContext is required");
      if (!platformContext.get())
      {
        return 1;
      }
      TutorialScenarioAppConfig config(platformContext.get(), settings);
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
