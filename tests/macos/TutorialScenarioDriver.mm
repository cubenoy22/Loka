#include "ScenarioDriverSupport.hpp"

#include "TutorialScenarioPresentation.hpp"
#include "TutorialScenarios.hpp"

namespace loka
{
  namespace macos_scenario_tests
  {
    namespace
    {
      class TutorialScenarioAppConfig : public scenario_tests::TutorialScenarioPresentation
      {
      public:
        TutorialScenarioAppConfig(PlatformContext *context,
                                  const dsl::SnapTestConfig::Settings &settings,
                                  ScenarioRunMode mode)
            : scenario_tests::TutorialScenarioPresentation(context,
                                                           scenario_tests::IsStartupScenario(settings.scenario)),
              runState_(settings, mode),
              driver_(scenario_tests::IsStartupScenario(settings.scenario),
                      scenario_tests::STARTUP_EXAMPLE_TUTORIAL,
                      &scenario_tests::MakeTutorialDriverErrorRecord,
                      2502,
                      this->runState_.audit()),
              borrowedApp_(0)
        {
        }

        void setApp(App *app)
        {
          this->borrowedApp_ = app;
        }

      private:
        virtual void onScenarioIdle(Window *window, double elapsedSeconds)
        {
          (void)elapsedSeconds;
          this->runState_.tick(window, this->borrowedApp_, this->driver_);
        }

        ScenarioRunState runState_;
        scenario_tests::SceneScenarioDriver<scenario_tests::TutorialScenario> driver_;
        App *borrowedApp_;
      };

      int RunTutorialScenarioApplication()
      {
        dsl::SnapTestConfig::Settings settings;
        ScenarioRunMode mode = SCENARIO_RUN_MODE_FLOW;
        if (!LoadScenarioSettings(settings, mode)
            || (!scenario_tests::IsStartupScenario(settings.scenario)
                && !scenario_tests::IsTutorialScenario(settings.scenario)))
        {
          std::fprintf(stderr, "macos Tutorial scenario is not registered\n");
          return 2;
        }
        return RunScenarioApplication<TutorialScenarioAppConfig>(settings, mode);
      }
    } // namespace
  } // namespace macos_scenario_tests
} // namespace loka

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  return loka::macos_scenario_tests::RunScenarioMain(&loka::macos_scenario_tests::RunTutorialScenarioApplication);
}
