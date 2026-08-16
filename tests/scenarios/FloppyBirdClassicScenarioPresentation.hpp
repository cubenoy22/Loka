#ifndef LOKA_TESTS_SCENARIOS_FLOPPY_BIRD_CLASSIC_SCENARIO_PRESENTATION_HPP
#define LOKA_TESTS_SCENARIOS_FLOPPY_BIRD_CLASSIC_SCENARIO_PRESENTATION_HPP

#include "../../example/FloppyBird/src/MyAppConfig.hpp"
#include "ObservedMainDefinition.hpp"

namespace loka
{
  namespace scenario_tests
  {
    /** Presents FloppyBird's production window and menu declarations around
        the deterministic Classic scenario game owner. */
    class FloppyBirdClassicScenarioPresentation : public ::MyAppConfig
    {
    public:
      FloppyBirdClassicScenarioPresentation(PlatformContext *context, unsigned long gameSeed)
          : ::MyAppConfig(context, gameSeed)
      {
      }

      virtual void compose(AppComposition &composition)
      {
        ObservedMainDefinition<floppybird::MainProps, floppybird::MainNode> mainDefinition(
            floppybird::MainProps(&this->gameModel()), 0);
        composition << WindowDef(this->productionWindowProps(mainDefinition)
                                     .idlePolicy(app::IdlePolicy::everyTick())
                                     .onIdle(&FloppyBirdClassicScenarioPresentation::OnWindowIdle, this));
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
        FloppyBirdClassicScenarioPresentation *self = static_cast<FloppyBirdClassicScenarioPresentation *>(userData);
        if (self)
        {
          self->onScenarioIdle(window, elapsedSeconds);
        }
      }
    };
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_FLOPPY_BIRD_CLASSIC_SCENARIO_PRESENTATION_HPP
