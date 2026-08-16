#include "ScenarioDriverSupport.hpp"

#include "ScrapbookScenarioPresentation.hpp"
#include "ScrapbookScenarios.hpp"

namespace loka
{
  namespace macos_scenario_tests
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
          (void)elapsedSeconds;
          this->runState_.tick(window, this->borrowedApp_, *this);
        }

        ScenarioRunState runState_;
        scenario_tests::ScrapbookScenario scenario_;
        App *borrowedApp_;
      };

      int RunScrapbookScenarioApplication()
      {
        dsl::SnapTestConfig::Settings settings;
        ScenarioRunMode mode = SCENARIO_RUN_MODE_FLOW;
        if (!LoadScenarioSettings(settings, mode))
        {
          return 2;
        }
        scenario_tests::ScenarioLaunchPlan launchPlan;
        if (!scenario_tests::QueryRigLaunchPlan(true, settings, launchPlan))
        {
          std::fprintf(stderr, "macos scenario: scenario is not registered\n");
          return 2;
        }

        platform::InitPlatformRuntime();
        core::ScopedPtr<PlatformContext> context(platform::CreatePlatformContext());
        assert(context.get() && "PlatformContext is required");
        if (!context.get())
        {
          return 1;
        }
        ScrapbookScenarioAppConfig config(context.get(), settings, launchPlan, mode);
        core::ScopedPtr<App> app(context->createApp(&config, 0, 0));
        assert(app.get() && "App is required");
        if (!app.get())
        {
          return 1;
        }
        config.setApp(app.get());
        app->run();
        return 0;
      }
    } // namespace
  } // namespace macos_scenario_tests
} // namespace loka

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  return loka::macos_scenario_tests::RunScenarioMain(&loka::macos_scenario_tests::RunScrapbookScenarioApplication);
}
