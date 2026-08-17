#include "MineSweeperScenarioLoop.hpp"

#include <cassert>

#include "MineSweeperScenarioPresentation.hpp"
#include "MineSweeperScenarios.hpp"
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
      platform::InitPlatformRuntime();
      core::ScopedPtr<PlatformContext> platformContext(platform::CreatePlatformContext());
      assert(platformContext.get() && "PlatformContext is required");
      if (!platformContext.get())
      {
        return 1;
      }
      MineSweeperScenarioLoopConfig config(platformContext.get());
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
