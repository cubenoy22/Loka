#include "ScenarioDriverSupport.hpp"

#include <cstdio>

#include "TutorialScenarioPresentation.hpp"
#include "TutorialScenarios.hpp"

#if !defined(TEST_BUILD)
#error LokaTutorialScenarioWin32 requires TEST_BUILD
#endif

namespace loka
{
  namespace win32_scenario_tests
  {
    namespace
    {
      class TutorialScenarioAppConfig : public scenario_tests::TutorialScenarioPresentation
      {
      public:
        TutorialScenarioAppConfig(PlatformContext *context,
                                  const dsl::SnapTestConfig::Settings &settings,
                                  ScenarioRunMode mode)
            : scenario_tests::TutorialScenarioPresentation(context,
                                                           scenario_tests::IsStartupScenario(settings.scenario)),
              runState_(settings, mode),
              driver_(scenario_tests::IsStartupScenario(settings.scenario),
                      scenario_tests::STARTUP_EXAMPLE_TUTORIAL,
                      &scenario_tests::MakeTutorialDriverErrorRecord,
                      2502,
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
        scenario_tests::SceneScenarioDriver<scenario_tests::TutorialScenario> driver_;
        App *borrowedApp_;
      };

      int RunTutorialScenarioApplication(HINSTANCE hInstance, int nCmdShow)
      {
        dsl::SnapTestConfig::Settings settings;
        ScenarioRunMode mode = SCENARIO_RUN_MODE_FLOW;
        if (!LoadScenarioSettings(settings, mode)
            || (!scenario_tests::IsStartupScenario(settings.scenario)
                && !scenario_tests::IsTutorialScenario(settings.scenario)))
        {
          const char *scenario = settings.hasScenario ? settings.scenario.c_str() : "startup";
          return WriteConfigurationErrorAudit(
              settings,
              scenario,
              scenario_tests::MakeTutorialDriverErrorRecord(2510, "Tutorial scenario is not registered"));
        }
        return RunScenarioApplication<TutorialScenarioAppConfig>(hInstance, nCmdShow, settings, mode);
      }
    } // namespace
  } // namespace win32_scenario_tests
} // namespace loka

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR commandLine, int nCmdShow)
{
  (void)hPrevInstance;
  (void)commandLine;
  return loka::win32_scenario_tests::RunTutorialScenarioApplication(hInstance, nCmdShow);
}
