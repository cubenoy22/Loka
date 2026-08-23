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
          runControl_("FloppyBird", diagnostics)
    {
    }

    FloppyBirdStandaloneFlowAppConfig::~FloppyBirdStandaloneFlowAppConfig()
    {
      this->scenario_.stop();
    }

    int FloppyBirdStandaloneFlowAppConfig::exitCode() const
    {
      return this->audit_.isValid() && !this->runControl_.failed() ? 0 : 1;
    }

    void FloppyBirdStandaloneFlowAppConfig::setApp(App *app)
    {
      this->runControl_.setApp(app);
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
      const StandaloneRunControl::Advance mountAdvance = this->runControl_.advance(this->borrowedMainNode_ != 0);
      switch (mountAdvance)
      {
      case StandaloneRunControl::ADVANCE_WAITING:
      case StandaloneRunControl::ADVANCE_FAILED:
        return;
      case StandaloneRunControl::ADVANCE_MOUNTED:
        break;
      }
      if (!window || !window->scene())
      {
        return;
      }
      this->game_.advanceFrame(loka_floppy_bird::kFixedStepSeconds);
      dsl::SnapRecord record;
      const scenario_tests::ScenarioAdvance advance =
          this->scenario_.step(this->runControl_.tick(),
                               window->scene(),
                               this->game_,
                               StandaloneContentBounds(window),
                               record);
      this->runControl_.observeScenarioAdvance(advance, record);
    }
  } // namespace standalone_tests
} // namespace loka
