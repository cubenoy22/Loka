#include "ScenarioDriverSupport.hpp"

#include "HelloWorldScenarioPresentation.hpp"
#include "HelloWorldScenarios.hpp"

namespace loka
{
  namespace macos_scenario_tests
  {
    namespace
    {
      class HelloWorldScenarioAppConfig : public scenario_tests::HelloWorldScenarioPresentation
      {
      public:
        HelloWorldScenarioAppConfig(PlatformContext *context,
                                    const dsl::SnapTestConfig::Settings &settings,
                                    ScenarioRunMode mode)
            : scenario_tests::HelloWorldScenarioPresentation(context, HelloWorldMenuSeed::FromWallClock(0x13579BDFUL)),
              runState_(settings, mode),
              driver_(scenario_tests::IsStartupScenario(settings.scenario),
                      scenario_tests::STARTUP_EXAMPLE_HELLO_WORLD,
                      &scenario_tests::MakeHelloWorldDriverErrorRecord,
                      2402,
                      this->runState_.audit()),
              borrowedApp_(0)
        {
        }

        void setApp(App *app)
        {
          this->borrowedApp_ = app;
        }

      private:
        virtual void onScenarioIdle(Window *window, double elapsedSeconds)
        {
          (void)elapsedSeconds;
          this->runState_.tick(window, this->borrowedApp_, this->driver_);
        }

        ScenarioRunState runState_;
        scenario_tests::SceneScenarioDriver<scenario_tests::HelloWorldScenario> driver_;
        App *borrowedApp_;
      };

      int RunHelloWorldScenarioApplication()
      {
        dsl::SnapTestConfig::Settings settings;
        ScenarioRunMode mode = SCENARIO_RUN_MODE_FLOW;
        if (!LoadScenarioSettings(settings, mode)
            || (!scenario_tests::IsStartupScenario(settings.scenario)
                && !scenario_tests::IsHelloWorldScenario(settings.scenario)))
        {
          std::fprintf(stderr, "macos HelloWorld scenario is not registered\n");
          return 2;
        }
        return RunScenarioApplication<HelloWorldScenarioAppConfig>(settings, mode);
      }
    } // namespace
  } // namespace macos_scenario_tests
} // namespace loka

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  return loka::macos_scenario_tests::RunScenarioMain(&loka::macos_scenario_tests::RunHelloWorldScenarioApplication);
}
