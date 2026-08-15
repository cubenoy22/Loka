#include "MineSweeperStandaloneFlowAppConfig.hpp"

#include "../scenarios/ScenarioWindow.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/Window.hpp"

namespace loka
{
  namespace standalone_tests
  {
    MineSweeperStandaloneFlowAppConfig::MineSweeperStandaloneFlowAppConfig(
        PlatformContext *context,
        const platform::file::FileHandle *auditFile,
        std::FILE *diagnostics)
        : AppConfigurable(context),
          audit_(auditFile ? *auditFile : ResolveStandaloneAuditFile(), "new-game-twice"),
          scenario_(scenario_tests::SCENARIO_COMPLETION_HOLD_FINAL_SCENE, &this->audit_),
          borrowedMainNode_(0),
          mountDeadline_("MineSweeper", diagnostics)
    {
    }

    MineSweeperStandaloneFlowAppConfig::~MineSweeperStandaloneFlowAppConfig()
    {
      this->scenario_.stop();
    }

    int MineSweeperStandaloneFlowAppConfig::exitCode() const
    {
      return this->audit_.isValid() && !this->mountDeadline_.failed() ? 0 : 1;
    }

    void MineSweeperStandaloneFlowAppConfig::setApp(App *app)
    {
      this->mountDeadline_.setApp(app);
    }

    void MineSweeperStandaloneFlowAppConfig::compose(AppComposition &composition)
    {
      composition << scenario_tests::MakeScenarioWindow<minesweeper::MainProps, minesweeper::MainNode>(
          minesweeper::MainProps(scenario_tests::MineSweeperScenarioSeed()),
          &this->borrowedMainNode_,
          220,
          240,
          "Loka MineSweeper Standalone Flow",
          app::IdlePolicy::interval(0.1),
          &MineSweeperStandaloneFlowAppConfig::OnWindowIdle,
          this);
    }

    void MineSweeperStandaloneFlowAppConfig::OnWindowIdle(Window *window,
                                                          double elapsedSeconds,
                                                          void *userData)
    {
      (void)elapsedSeconds;
      MineSweeperStandaloneFlowAppConfig *self =
          static_cast<MineSweeperStandaloneFlowAppConfig *>(userData);
      if (self)
      {
        self->tick(window);
      }
    }

    void MineSweeperStandaloneFlowAppConfig::tick(Window *window)
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
      dsl::SnapRecord record;
      const scenario_tests::ScenarioAdvance advance =
          this->scenario_.step(this->mountDeadline_.tick(),
                               window->scene(),
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
