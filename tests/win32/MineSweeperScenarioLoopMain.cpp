#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ScenarioDriverSupport.hpp"

#include "MineSweeperScenarioPresentation.hpp"
#include "MineSweeperScenarios.hpp"
#include "ScenarioLoopAppConfig.hpp"

#if !defined(TEST_BUILD)
#error LokaMineSweeperScenarioLoopWin32 requires TEST_BUILD
#endif

namespace loka
{
  namespace win32_scenario_tests
  {
    namespace
    {
      typedef scenario_tests::ScenarioLoopAppConfig<scenario_tests::MineSweeperScenarioPresentation,
                                                    scenario_tests::MineSweeperScenario>
          MineSweeperScenarioLoopBase;

      class MineSweeperScenarioLoopConfig : public MineSweeperScenarioLoopBase
      {
      public:
        explicit MineSweeperScenarioLoopConfig(PlatformContext *context)
            : MineSweeperScenarioLoopBase(context,
                                          minesweeper::MainProps(scenario_tests::MineSweeperScenarioSeed()),
                                          scenario_tests::MineSweeperReelCells(),
                                          scenario_tests::STARTUP_EXAMPLE_MINESWEEPER,
                                          &scenario_tests::MakeMineSweeperDriverErrorRecord,
                                          2602,
                                          LOKA_SCENARIO_LOOP_HOLD_SECONDS,
                                          LOKA_SCENARIO_LOOP_CYCLES)
        {
        }
      };
    } // namespace

    int RunMineSweeperScenarioLoopApplication(HINSTANCE hInstance, int nCmdShow)
    {
      return RunScenarioApplication<MineSweeperScenarioLoopConfig>(hInstance, nCmdShow);
    }
  } // namespace win32_scenario_tests
} // namespace loka

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR commandLine, int nCmdShow)
{
  (void)hPrevInstance;
  (void)commandLine;
  return loka::win32_scenario_tests::RunMineSweeperScenarioLoopApplication(hInstance, nCmdShow);
}
