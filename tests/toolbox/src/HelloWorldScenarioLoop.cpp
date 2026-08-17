#include "HelloWorldScenarioLoop.hpp"

#include <cassert>

#include "HelloWorldScenarioPresentation.hpp"
#include "HelloWorldScenarios.hpp"
#include "ScenarioLoopAppConfig.hpp"
#include "app/PlatformContext.hpp"
#include "app/bootstrap/PlatformBootstrap.hpp"
#include "app/core/App.hpp"
#include "core/util/ScopedPtr.hpp"

namespace loka
{
  namespace toolbox_tests
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
      platform::InitPlatformRuntime();
      core::ScopedPtr<PlatformContext> platformContext(platform::CreatePlatformContext());
      assert(platformContext.get() && "PlatformContext is required");
      if (!platformContext.get())
      {
        return 1;
      }
      HelloWorldScenarioLoopConfig config(platformContext.get());
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
