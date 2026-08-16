#include "FloppyBirdStandaloneFlowAppConfig.hpp"

#include "../scenarios/ScenarioWindow.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/Window.hpp"

namespace loka
{
  namespace standalone_tests
  {
    FloppyBirdStandaloneFlowAppConfig::FloppyBirdStandaloneFlowAppConfig(
        PlatformContext *context,
        const platform::file::FileHandle *auditFile,
        std::FILE *diagnostics)
        : AppConfigurable(context),
          audit_(auditFile ? *auditFile : ResolveStandaloneAuditFile(), "fixed-step-flaps"),
          scenario_(scenario_tests::SCENARIO_COMPLETION_HOLD_FINAL_SCENE, &this->audit_),
          game_(scenario_tests::FloppyBirdScenarioSeed()),
          borrowedMainNode_(0),
          mountDeadline_("FloppyBird", diagnostics)
    {
    }

    FloppyBirdStandaloneFlowAppConfig::~FloppyBirdStandaloneFlowAppConfig()
    {
      this->scenario_.stop();
    }

    int FloppyBirdStandaloneFlowAppConfig::exitCode() const
    {
      return this->audit_.isValid() && !this->mountDeadline_.failed() ? 0 : 1;
    }

    void FloppyBirdStandaloneFlowAppConfig::setApp(App *app)
    {
      this->mountDeadline_.setApp(app);
    }

    void FloppyBirdStandaloneFlowAppConfig::compose(AppComposition &composition)
    {
      composition << scenario_tests::MakeScenarioWindow<floppybird::MainProps, floppybird::MainNode>(
          floppybird::MainProps(&this->game_),
          &this->borrowedMainNode_,
          380,
          340,
          "Loka FloppyBird Standalone Flow",
          app::IdlePolicy::everyTick(),
          &FloppyBirdStandaloneFlowAppConfig::OnWindowIdle,
          this);
    }

    void FloppyBirdStandaloneFlowAppConfig::OnWindowIdle(Window *window,
                                                         double elapsedSeconds,
                                                         void *userData)
    {
      (void)elapsedSeconds;
      FloppyBirdStandaloneFlowAppConfig *self =
          static_cast<FloppyBirdStandaloneFlowAppConfig *>(userData);
      if (self)
      {
        self->tick(window);
      }
    }

    void FloppyBirdStandaloneFlowAppConfig::tick(Window *window)
    {
      const StandaloneMountDeadline::Advance mountAdvance =
          this->mountDeadline_.advance(this->borrowedMainNode_ != 0);
      switch (mountAdvance)
      {
      case StandaloneMountDeadline::ADVANCE_WAITING:
      case StandaloneMountDeadline::ADVANCE_FAILED:
        return;
      case StandaloneMountDeadline::ADVANCE_MOUNTED:
        break;
      }
      if (!window || !window->scene())
      {
        return;
      }
      this->game_.advanceFrame(loka_floppy_bird::kFixedStepSeconds);
      dsl::SnapRecord record;
      const scenario_tests::ScenarioAdvance advance =
          this->scenario_.step(this->mountDeadline_.tick(),
                               window->scene(),
                               this->game_,
                               StandaloneContentBounds(window),
                               record);
      switch (advance)
      {
      case scenario_tests::SCENARIO_ADVANCE_PENDING:
      case scenario_tests::SCENARIO_ADVANCE_FINAL_SCENE_HELD:
      case scenario_tests::SCENARIO_ADVANCE_DRIVER_COMPLETION_READY:
        return;
      }
    }
  } // namespace standalone_tests
} // namespace loka
