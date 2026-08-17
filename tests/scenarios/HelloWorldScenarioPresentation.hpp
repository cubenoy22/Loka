#ifndef LOKA_TESTS_SCENARIOS_HELLO_WORLD_SCENARIO_PRESENTATION_HPP
#define LOKA_TESTS_SCENARIOS_HELLO_WORLD_SCENARIO_PRESENTATION_HPP

#include "../../example/HelloWorld/src/ProductionAppConfig.hpp"
#include "ObservedMainDefinition.hpp"
#include "ScenarioWindowPresentation.hpp"

namespace loka
{
  namespace scenario_tests
  {
    /** Presents HelloWorld's production window and menu declarations while
        leaving the TEST vehicle responsible for scenario tick handling. */
    class HelloWorldScenarioPresentation : public HelloWorldProductionAppConfig,
                                           protected ScenarioWindowPresentation
    {
    public:
      explicit HelloWorldScenarioPresentation(PlatformContext *context)
          : HelloWorldProductionAppConfig(context)
      {
      }

      HelloWorldScenarioPresentation(PlatformContext *context, const HelloWorldMenuSeed &menuSeed)
          : HelloWorldProductionAppConfig(context, menuSeed)
      {
      }

      virtual void compose(AppComposition &composition)
      {
        ObservedMainDefinition<helloworld::MainProps, helloworld::MainNode> mainDefinition(helloworld::MainProps(), 0);
        WindowProps windowProps = this->productionWindowProps(mainDefinition);
        this->applyScenarioWindowPresentation(windowProps);
        composition << WindowDef(windowProps.idlePolicy(app::IdlePolicy::everyTick())
                                     .onIdle(&HelloWorldScenarioPresentation::OnWindowIdle, this));
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
        HelloWorldScenarioPresentation *self = static_cast<HelloWorldScenarioPresentation *>(userData);
        if (self)
        {
          self->onScenarioIdle(window, elapsedSeconds);
        }
      }
    };
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_HELLO_WORLD_SCENARIO_PRESENTATION_HPP
