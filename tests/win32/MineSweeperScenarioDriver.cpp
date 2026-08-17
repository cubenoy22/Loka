#include "ScenarioDriverSupport.hpp"

#include <cstdio>

#include "MineSweeperScenarioPresentation.hpp"
#include "MineSweeperScenarios.hpp"

#if !defined(TEST_BUILD)
#error LokaMineSweeperScenarioWin32 requires TEST_BUILD
#endif

namespace loka
{
  namespace win32_scenario_tests
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
                      settings.scenario,
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

        int exitCode() const
        {
          return this->runState_.exitCode();
        }

      private:
        virtual void onScenarioIdle(Window *window, double elapsedSeconds)
        {
          this->runState_.tick(window, this->borrowedApp_, this->driver_, elapsedSeconds);
        }

        ScenarioRunState runState_;
        scenario_tests::SceneScenarioDriver<scenario_tests::MineSweeperScenario> driver_;
        App *borrowedApp_;
      };

      int RunMineSweeperScenarioApplication(HINSTANCE hInstance, int nCmdShow)
      {
        dsl::SnapTestConfig::Settings settings;
        ScenarioRunMode mode = SCENARIO_RUN_MODE_FLOW;
        if (!LoadScenarioSettings(settings, mode)
            || (!scenario_tests::IsStartupScenario(settings.scenario)
                && !scenario_tests::IsMineSweeperScenario(settings.scenario)))
        {
          const char *scenario = settings.hasScenario ? settings.scenario.c_str() : "startup";
          return WriteConfigurationErrorAudit(
              settings,
              scenario,
              scenario_tests::MakeMineSweeperDriverErrorRecord(2610, "MineSweeper scenario is not registered"));
        }
        return RunScenarioApplication<MineSweeperScenarioAppConfig>(hInstance, nCmdShow, settings, mode);
      }
    } // namespace
  } // namespace win32_scenario_tests
} // namespace loka

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR commandLine, int nCmdShow)
{
  (void)hPrevInstance;
  (void)commandLine;
  return loka::win32_scenario_tests::RunMineSweeperScenarioApplication(hInstance, nCmdShow);
}
