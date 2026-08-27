#ifndef LOKA_TESTS_SCENARIOS_MINESWEEPER_SCENARIO_PRESENTATION_HPP
#define LOKA_TESTS_SCENARIOS_MINESWEEPER_SCENARIO_PRESENTATION_HPP

#include "../../example/MineSweeper/src/MyAppConfig.hpp"
#include "ObservedMainDefinition.hpp"
#include "ScenarioWindowPresentation.hpp"

namespace loka
{
  namespace scenario_tests
  {
    /** Presents MineSweeper's production window declaration around its
        deterministic scenario scene. */
    class MineSweeperScenarioPresentation : public ::MineSweeperAppConfig,
                                            protected ScenarioWindowPresentation
    {
    public:
      MineSweeperScenarioPresentation(PlatformContext *context, const minesweeper::MainProps &mainProps)
          : ::MineSweeperAppConfig(context, mainProps)
      {
      }

      virtual void compose(AppComposition &composition)
      {
        ObservedMainDefinition<minesweeper::MainProps, minesweeper::MainNode> mainDefinition(this->mainProps(), 0);
        WindowProps windowProps = this->productionWindowProps(mainDefinition);
        this->applyScenarioWindowPresentation(windowProps);
        composition << WindowDef(windowProps.idlePolicy(app::IdlePolicy::everyTick())
                                     .onIdle(&MineSweeperScenarioPresentation::OnWindowIdle, this));
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
        MineSweeperScenarioPresentation *self = static_cast<MineSweeperScenarioPresentation *>(userData);
        if (self)
        {
          self->onScenarioIdle(window, elapsedSeconds);
        }
      }
    };
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_MINESWEEPER_SCENARIO_PRESENTATION_HPP
