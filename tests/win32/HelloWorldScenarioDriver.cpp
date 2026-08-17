#include "ScenarioDriverSupport.hpp"

#include <cstdio>

#include "HelloWorldScenarioPresentation.hpp"
#include "HelloWorldScenarios.hpp"

#if !defined(TEST_BUILD)
#error LokaHelloWorldScenarioWin32 requires TEST_BUILD
#endif

namespace loka
{
  namespace win32_scenario_tests
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

        int exitCode() const
        {
          return this->runState_.exitCode();
        }

      private:
        virtual void onScenarioIdle(Window *window, double elapsedSeconds)
        {
          this->runState_.tick(window, this->borrowedApp_, this->driver_, elapsedSeconds);
        }

        ScenarioRunState runState_;
        scenario_tests::SceneScenarioDriver<scenario_tests::HelloWorldScenario> driver_;
        App *borrowedApp_;
      };

      int RunHelloWorldScenarioApplication(HINSTANCE hInstance, int nCmdShow)
      {
        dsl::SnapTestConfig::Settings settings;
        ScenarioRunMode mode = SCENARIO_RUN_MODE_FLOW;
        if (!LoadScenarioSettings(settings, mode)
            || (!scenario_tests::IsStartupScenario(settings.scenario)
                && !scenario_tests::IsHelloWorldScenario(settings.scenario)))
        {
          const char *scenario = settings.hasScenario ? settings.scenario.c_str() : "startup";
          return WriteConfigurationErrorAudit(
              settings,
              scenario,
              scenario_tests::MakeHelloWorldDriverErrorRecord(2410, "HelloWorld scenario is not registered"));
        }
        return RunScenarioApplication<HelloWorldScenarioAppConfig>(hInstance, nCmdShow, settings, mode);
      }
    } // namespace
  } // namespace win32_scenario_tests
} // namespace loka

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR commandLine, int nCmdShow)
{
  (void)hPrevInstance;
  (void)commandLine;
  return loka::win32_scenario_tests::RunHelloWorldScenarioApplication(hInstance, nCmdShow);
}
