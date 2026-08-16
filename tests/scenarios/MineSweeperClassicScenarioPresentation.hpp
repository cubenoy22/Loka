#ifndef LOKA_TESTS_SCENARIOS_MINESWEEPER_CLASSIC_SCENARIO_PRESENTATION_HPP
#define LOKA_TESTS_SCENARIOS_MINESWEEPER_CLASSIC_SCENARIO_PRESENTATION_HPP

#include "../../example/MineSweeper/src/MyAppConfig.hpp"
#include "ObservedMainDefinition.hpp"

namespace loka
{
  namespace scenario_tests
  {
    /** Presents MineSweeper's production window declaration around its
        deterministic Classic scenario scene. */
    class MineSweeperClassicScenarioPresentation : public ::MyAppConfig
    {
    public:
      MineSweeperClassicScenarioPresentation(PlatformContext *context, const minesweeper::MainProps &mainProps)
          : ::MyAppConfig(context, mainProps)
      {
      }

      virtual void compose(AppComposition &composition)
      {
        ObservedMainDefinition<minesweeper::MainProps, minesweeper::MainNode> mainDefinition(this->mainProps(), 0);
        composition << WindowDef(this->productionWindowProps(mainDefinition)
                                     .idlePolicy(app::IdlePolicy::everyTick())
                                     .onIdle(&MineSweeperClassicScenarioPresentation::OnWindowIdle, this));
      }

    protected:
      virtual void onScenarioIdle(Window *window, double elapsedSeconds)
      {
        (void)window;
        (void)elapsedSeconds;
      }

    private:
      static void OnWindowIdle(Window *window, double elapsedSeconds, void *userData)
      {
        MineSweeperClassicScenarioPresentation *self = static_cast<MineSweeperClassicScenarioPresentation *>(userData);
        if (self)
        {
          self->onScenarioIdle(window, elapsedSeconds);
        }
      }
    };
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_MINESWEEPER_CLASSIC_SCENARIO_PRESENTATION_HPP
