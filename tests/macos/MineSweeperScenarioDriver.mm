#include "ScenarioDriverSupport.hpp"

#include "MineSweeperScenarioPresentation.hpp"
#include "MineSweeperScenarios.hpp"

namespace loka
{
  namespace macos_scenario_tests
  {
    namespace
    {
      class MineSweeperScenarioAppConfig : public scenario_tests::MineSweeperScenarioPresentation
      {
      public:
        MineSweeperScenarioAppConfig(PlatformContext *context,
                                     const dsl::SnapTestConfig::Settings &settings,
                                     ScenarioRunMode mode)
            : scenario_tests::MineSweeperScenarioPresentation(
                  context, minesweeper::MainProps(scenario_tests::MineSweeperScenarioSeed())),
              runState_(settings, mode),
              driver_(scenario_tests::IsStartupScenario(settings.scenario),
                      scenario_tests::STARTUP_EXAMPLE_MINESWEEPER,
                      &scenario_tests::MakeMineSweeperDriverErrorRecord,
                      2602,
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
        scenario_tests::SceneScenarioDriver<scenario_tests::MineSweeperScenario> driver_;
        App *borrowedApp_;
      };

      int RunMineSweeperScenarioApplication()
      {
        dsl::SnapTestConfig::Settings settings;
        ScenarioRunMode mode = SCENARIO_RUN_MODE_FLOW;
        if (!LoadScenarioSettings(settings, mode)
            || (!scenario_tests::IsStartupScenario(settings.scenario)
                && !scenario_tests::IsMineSweeperScenario(settings.scenario)))
        {
          std::fprintf(stderr, "macos MineSweeper scenario is not registered\n");
          return 2;
        }
        return RunScenarioApplication<MineSweeperScenarioAppConfig>(settings, mode);
      }
    } // namespace
  } // namespace macos_scenario_tests
} // namespace loka

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  return loka::macos_scenario_tests::RunScenarioMain(&loka::macos_scenario_tests::RunMineSweeperScenarioApplication);
}
