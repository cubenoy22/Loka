#include "ScenarioDriverSupport.hpp"
#include "SmirkBenchAttributedScenario.hpp"
#include "SmirkBenchScenarioPresentation.hpp"

namespace loka
{
  namespace win32_scenario_tests
  {
    namespace
    {
      // Deliberate rail twins: only run-state ticking and the entry point differ.
      class SmirkBenchScenarioAppConfig : public scenario_tests::SmirkBenchScenarioPresentation
      {
      public:
        SmirkBenchScenarioAppConfig(PlatformContext *context,
                                    const dsl::SnapTestConfig::Settings &settings,
                                    ScenarioRunMode mode)
            : scenario_tests::SmirkBenchScenarioPresentation(context, settings.scenario == "attributed-editor-line"),
              runState_(settings, mode),
              driver_(settings.scenario == "startup", this->runState_.audit()),
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
        scenario_tests::SmirkBenchEditorDriver driver_;
        App *borrowedApp_;
      };

      int RunSmirkBenchScenarioApplication(HINSTANCE instance, int show)
      {
        dsl::SnapTestConfig::Settings settings;
        ScenarioRunMode mode = SCENARIO_RUN_MODE_FLOW;
        if (!LoadScenarioSettings(settings, mode)
            || (settings.scenario != "startup" && settings.scenario != "attributed-editor-line"))
        {
          return 2;
        }
        return RunScenarioApplication<SmirkBenchScenarioAppConfig>(instance, show, settings, mode);
      }
    } // namespace
  } // namespace win32_scenario_tests
} // namespace loka
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR commandLine, int show)
{
  (void)previous;
  (void)commandLine;
  return loka::win32_scenario_tests::RunSmirkBenchScenarioApplication(instance, show);
}
