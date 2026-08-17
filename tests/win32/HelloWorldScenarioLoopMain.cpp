#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ScenarioDriverSupport.hpp"

#include "HelloWorldScenarioPresentation.hpp"
#include "HelloWorldScenarios.hpp"
#include "ScenarioLoopAppConfig.hpp"

#if !defined(TEST_BUILD)
#error LokaHelloWorldScenarioLoopWin32 requires TEST_BUILD
#endif

namespace loka
{
  namespace win32_scenario_tests
  {
    namespace
    {
      typedef scenario_tests::ScenarioLoopAppConfig<scenario_tests::HelloWorldScenarioPresentation,
                                                    scenario_tests::HelloWorldScenario>
          HelloWorldScenarioLoopBase;

      class HelloWorldScenarioLoopConfig : public HelloWorldScenarioLoopBase
      {
      public:
        explicit HelloWorldScenarioLoopConfig(PlatformContext *context)
            : HelloWorldScenarioLoopBase(context,
                                         HelloWorldMenuSeed::FromWallClock(0x13579BDFUL),
                                         scenario_tests::HelloWorldReelCells(),
                                         scenario_tests::STARTUP_EXAMPLE_HELLO_WORLD,
                                         &scenario_tests::MakeHelloWorldDriverErrorRecord,
                                         2402,
                                         LOKA_SCENARIO_LOOP_HOLD_SECONDS,
                                         LOKA_SCENARIO_LOOP_CYCLES)
        {
        }
      };
    } // namespace

    int RunHelloWorldScenarioLoopApplication(HINSTANCE hInstance, int nCmdShow)
    {
      return RunScenarioApplication<HelloWorldScenarioLoopConfig>(hInstance, nCmdShow);
    }
  } // namespace win32_scenario_tests
} // namespace loka

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR commandLine, int nCmdShow)
{
  (void)hPrevInstance;
  (void)commandLine;
  return loka::win32_scenario_tests::RunHelloWorldScenarioLoopApplication(hInstance, nCmdShow);
}
