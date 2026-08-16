#ifndef LOKA_TESTS_SCENARIOS_HELLO_WORLD_CLASSIC_SCENARIO_PRESENTATION_HPP
#define LOKA_TESTS_SCENARIOS_HELLO_WORLD_CLASSIC_SCENARIO_PRESENTATION_HPP

#include "../../example/HelloWorld/src/ProductionAppConfig.hpp"
#include "ObservedMainDefinition.hpp"

namespace loka
{
  namespace scenario_tests
  {
    /** Presents HelloWorld's production window and menu declarations while
        leaving the TEST vehicle responsible for scenario tick handling. */
    class HelloWorldClassicScenarioPresentation : public HelloWorldProductionAppConfig
    {
    public:
      explicit HelloWorldClassicScenarioPresentation(PlatformContext *context)
          : HelloWorldProductionAppConfig(context)
      {
      }

      HelloWorldClassicScenarioPresentation(PlatformContext *context,
                                            const HelloWorldMenuSeed &menuSeed)
          : HelloWorldProductionAppConfig(context, menuSeed)
      {
      }

      virtual void compose(AppComposition &composition)
      {
        ObservedMainDefinition<helloworld::MainProps, helloworld::MainNode> mainDefinition(
            helloworld::MainProps(), 0);
        composition << WindowDef(this->productionWindowProps(mainDefinition)
                                     .idlePolicy(app::IdlePolicy::everyTick())
                                     .onIdle(&HelloWorldClassicScenarioPresentation::OnWindowIdle, this));
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
        HelloWorldClassicScenarioPresentation *self =
            static_cast<HelloWorldClassicScenarioPresentation *>(userData);
        if (self)
        {
          self->onScenarioIdle(window, elapsedSeconds);
        }
      }
    };
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_HELLO_WORLD_CLASSIC_SCENARIO_PRESENTATION_HPP
