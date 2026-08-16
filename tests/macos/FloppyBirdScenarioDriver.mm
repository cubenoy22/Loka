#include "ScenarioDriverSupport.hpp"

#include "FloppyBirdScenarioPresentation.hpp"
#include "FloppyBirdScenarios.hpp"

namespace loka
{
  namespace macos_scenario_tests
  {
    namespace
    {
      class FloppyBirdScenarioDriver : public scenario_tests::ScenarioDriver
      {
      public:
        FloppyBirdScenarioDriver(bool startup, floppybird::GameModel &game, dsl::testing::ScenarioAuditSink *audit)
            : startup_(startup),
              game_(game),
              startupScenario_(
                  scenario_tests::STARTUP_EXAMPLE_FLOPPY_BIRD, scenario_tests::SCENARIO_COMPLETION_DRIVER_OWNED, audit),
              scenario_(scenario_tests::SCENARIO_COMPLETION_DRIVER_OWNED, audit)
        {
        }

        virtual ~FloppyBirdScenarioDriver()
        {
          if (this->startup_)
          {
            this->startupScenario_.stop();
          }
          else
          {
            this->scenario_.stop();
          }
        }

        virtual scenario_tests::ScenarioAdvance
        step(long tick, Window *window, const scenario_tests::CaptureContentBounds &bounds, dsl::SnapRecord &out)
        {
          if (!window || !window->scene())
          {
            out = this->startup_ ? scenario_tests::MakeStartupDriverErrorRecord(
                                       scenario_tests::STARTUP_EXAMPLE_FLOPPY_BIRD, 2802, "Scene was not mounted")
                                 : scenario_tests::MakeFloppyBirdDriverErrorRecord(2702, "Scene was not mounted");
            return scenario_tests::SCENARIO_ADVANCE_DRIVER_COMPLETION_READY;
          }
          this->game_.advanceFrame(loka_floppy_bird::kFixedStepSeconds);
          return this->startup_ ? this->startupScenario_.step(tick, window->scene(), bounds, out)
                                : this->scenario_.step(tick, window->scene(), this->game_, bounds, out);
        }

        virtual bool publishVerdict(const dsl::SnapRecord &record)
        {
          return this->startup_ ? this->startupScenario_.publishVerdict(record)
                                : this->scenario_.publishVerdict(record);
        }

      private:
        const bool startup_;
        floppybird::GameModel &game_;
        scenario_tests::StartupScenario startupScenario_;
        scenario_tests::FloppyBirdScenario scenario_;
      };

      class FloppyBirdScenarioAppConfig : public scenario_tests::FloppyBirdScenarioPresentation
      {
      public:
        FloppyBirdScenarioAppConfig(PlatformContext *context,
                                    const dsl::SnapTestConfig::Settings &settings,
                                    ScenarioRunMode mode)
            : scenario_tests::FloppyBirdScenarioPresentation(context, scenario_tests::FloppyBirdScenarioSeed()),
              runState_(settings, mode),
              driver_(scenario_tests::IsStartupScenario(settings.scenario), this->gameModel(), this->runState_.audit()),
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
        FloppyBirdScenarioDriver driver_;
        App *borrowedApp_;
      };

      int RunFloppyBirdScenarioApplication()
      {
        dsl::SnapTestConfig::Settings settings;
        ScenarioRunMode mode = SCENARIO_RUN_MODE_FLOW;
        if (!LoadScenarioSettings(settings, mode)
            || (!scenario_tests::IsStartupScenario(settings.scenario)
                && !scenario_tests::IsFloppyBirdScenario(settings.scenario)))
        {
          std::fprintf(stderr, "macos FloppyBird scenario is not registered\n");
          return 2;
        }
        return RunScenarioApplication<FloppyBirdScenarioAppConfig>(settings, mode);
      }
    } // namespace
  } // namespace macos_scenario_tests
} // namespace loka

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  return loka::macos_scenario_tests::RunScenarioMain(&loka::macos_scenario_tests::RunFloppyBirdScenarioApplication);
}
