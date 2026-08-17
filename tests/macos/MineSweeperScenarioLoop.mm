#include <Foundation/Foundation.h>

#include "ScenarioDriverSupport.hpp"

#include "MineSweeperScenarioPresentation.hpp"
#include "MineSweeperScenarios.hpp"
#include "ScenarioLoopAppConfig.hpp"

#if !defined(TEST_BUILD)
#error LokaMineSweeperScenarioLoopMacOS requires TEST_BUILD
#endif

namespace loka
{
  namespace macos_scenario_tests
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

    int RunMineSweeperScenarioLoopApplication()
    {
      return RunScenarioApplication<MineSweeperScenarioLoopConfig>();
    }
  } // namespace macos_scenario_tests
} // namespace loka

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  const int result = loka::macos_scenario_tests::RunMineSweeperScenarioLoopApplication();
  (void)pool;
  return result;
}
