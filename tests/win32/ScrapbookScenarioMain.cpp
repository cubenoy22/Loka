#include "ScenarioDriverSupport.hpp"

#include <cstdio>

#include "ScrapbookScenarioPresentation.hpp"
#include "ScrapbookScenarios.hpp"

#if !defined(TEST_BUILD)
#error LokaScrapbookScenarioWin32 requires TEST_BUILD
#endif

namespace loka
{
  namespace win32_scenario_tests
  {
    namespace
    {
      class ScrapbookScenarioAppConfig : public scenario_tests::ScrapbookScenarioPresentation,
                                         public scenario_tests::ScenarioDriver
      {
      public:
        ScrapbookScenarioAppConfig(PlatformContext *context,
                                   const dsl::SnapTestConfig::Settings &settings,
                                   const scenario_tests::ScenarioLaunchPlan &launchPlan,
                                   ScenarioRunMode mode)
            : scenario_tests::ScrapbookScenarioPresentation(context),
              runState_(settings, mode),
              scenario_(launchPlan, this->runState_.audit()),
              borrowedApp_(0)
        {
        }

        virtual ~ScrapbookScenarioAppConfig()
        {
          this->scenario_.stop();
        }

        void setApp(App *app)
        {
          this->borrowedApp_ = app;
        }

        int exitCode() const
        {
          return this->runState_.exitCode();
        }

        virtual scenario_tests::ScenarioAdvance
        step(long tick, Window *window, const scenario_tests::CaptureContentBounds &bounds, dsl::SnapRecord &out)
        {
          if (!this->observedMainNode())
          {
            out =
                scenario_tests::MakeDriverErrorRecord(this->scenario_.name().c_str(), 2303, "MainNode was not mounted");
            return scenario_tests::SCENARIO_ADVANCE_DRIVER_COMPLETION_READY;
          }
          return this->scenario_.step(tick, window ? window->scene() : 0, *this->observedMainNode(), bounds, out);
        }

        virtual bool publishVerdict(const dsl::SnapRecord &record)
        {
          return this->scenario_.publishVerdict(record);
        }

      private:
        virtual void onScenarioIdle(Window *window, double elapsedSeconds)
        {
          this->runState_.tick(window, this->borrowedApp_, *this, elapsedSeconds);
        }

        ScenarioRunState runState_;
        scenario_tests::ScrapbookScenario scenario_;
        App *borrowedApp_;
      };

      int RunScrapbookScenarioApplication(HINSTANCE hInstance, int nCmdShow)
      {
        dsl::SnapTestConfig::Settings settings;
        ScenarioRunMode mode = SCENARIO_RUN_MODE_FLOW;
        if (!LoadScenarioSettings(settings, mode))
        {
          const char *scenario = settings.hasScenario ? settings.scenario.c_str() : "startup";
          return WriteConfigurationErrorAudit(
              settings,
              scenario,
              scenario_tests::MakeDriverErrorRecord(scenario, 2310, "LokaTest.cfg is missing or invalid"));
        }
        scenario_tests::ScenarioLaunchPlan launchPlan;
        if (!scenario_tests::QueryRigLaunchPlan(true, settings, launchPlan))
        {
          return WriteConfigurationErrorAudit(
              settings,
              settings.scenario.c_str(),
              scenario_tests::MakeDriverErrorRecord(
                  settings.scenario.c_str(), 2311, "ScrapbookUI scenario is not registered"));
        }

        platform::InitPlatformRuntime();
        core::ScopedPtr<PlatformContext> context(platform::CreatePlatformContext());
        assert(context.get() && "PlatformContext is required");
        if (!context.get())
        {
          return 1;
        }
        ScrapbookScenarioAppConfig config(context.get(), settings, launchPlan, mode);
        core::ScopedPtr<App> app(context->createApp(&config, hInstance, nCmdShow));
        assert(app.get() && "App is required");
        if (!app.get())
        {
          return 1;
        }
        config.setApp(app.get());
        app->run();
        return config.exitCode();
      }
    } // namespace
  } // namespace win32_scenario_tests
} // namespace loka

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR commandLine, int nCmdShow)
{
  (void)hPrevInstance;
  (void)commandLine;
  return loka::win32_scenario_tests::RunScrapbookScenarioApplication(hInstance, nCmdShow);
}
