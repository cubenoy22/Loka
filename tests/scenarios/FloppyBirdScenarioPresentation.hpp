#ifndef LOKA_TESTS_SCENARIOS_FLOPPY_BIRD_SCENARIO_PRESENTATION_HPP
#define LOKA_TESTS_SCENARIOS_FLOPPY_BIRD_SCENARIO_PRESENTATION_HPP

#include "../../example/FloppyBird/src/MyAppConfig.hpp"
#include "ObservedMainDefinition.hpp"

namespace loka
{
  namespace scenario_tests
  {
    /** Presents FloppyBird's production window and menu declarations around
        the deterministic scenario game owner. */
    class FloppyBirdScenarioPresentation : public ::FloppyBirdAppConfig
    {
    public:
      FloppyBirdScenarioPresentation(PlatformContext *context, unsigned long gameSeed)
          : ::FloppyBirdAppConfig(context, gameSeed)
      {
      }

      virtual void compose(AppComposition &composition)
      {
        ObservedMainDefinition<floppybird::MainProps, floppybird::MainNode> mainDefinition(
            floppybird::MainProps(&this->gameModel()), 0);
        composition << WindowDef(this->productionWindowProps(mainDefinition)
                                     .idlePolicy(app::IdlePolicy::everyTick())
                                     .onIdle(&FloppyBirdScenarioPresentation::OnWindowIdle, this));
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
        FloppyBirdScenarioPresentation *self = static_cast<FloppyBirdScenarioPresentation *>(userData);
        if (self)
        {
          self->onScenarioIdle(window, elapsedSeconds);
        }
      }
    };
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_FLOPPY_BIRD_SCENARIO_PRESENTATION_HPP
