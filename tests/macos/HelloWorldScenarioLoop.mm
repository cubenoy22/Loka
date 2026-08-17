#include <Foundation/Foundation.h>

#include "ScenarioDriverSupport.hpp"

#include "HelloWorldScenarioPresentation.hpp"
#include "HelloWorldScenarios.hpp"
#include "ScenarioLoopAppConfig.hpp"

#if !defined(TEST_BUILD)
#error LokaHelloWorldScenarioLoopMacOS requires TEST_BUILD
#endif

namespace loka
{
  namespace macos_scenario_tests
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

    int RunHelloWorldScenarioLoopApplication()
    {
      return RunScenarioApplication<HelloWorldScenarioLoopConfig>();
    }
  } // namespace macos_scenario_tests
} // namespace loka

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  const int result = loka::macos_scenario_tests::RunHelloWorldScenarioLoopApplication();
  (void)pool;
  return result;
}
