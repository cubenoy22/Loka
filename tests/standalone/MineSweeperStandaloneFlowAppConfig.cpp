#include "MineSweeperStandaloneFlowAppConfig.hpp"

#include "../scenarios/ScenarioWindow.hpp"
#include "../scenarios/ScenarioReel.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/Window.hpp"

namespace loka
{
  namespace standalone_tests
  {
    namespace
    {
      const char *ConfiguredMineSweeperScenarioName()
      {
#if LOKA_STANDALONE_FLOW_LOOP
        return "seeded-reveal";
#else
        return "new-game-twice";
#endif
      }
    } // namespace

    MineSweeperStandaloneFlowAppConfig::MineSweeperStandaloneFlowAppConfig(
        PlatformContext *context,
        const platform::file::FileHandle *auditFile,
        std::FILE *diagnostics)
        : AppConfigurable(context),
          audit_(auditFile ? *auditFile : ResolveStandaloneAuditFile(), ConfiguredMineSweeperScenarioName()),
          scenario_(new (std::nothrow) scenario_tests::MineSweeperScenario(
              ConfiguredMineSweeperScenarioName(),
              scenario_tests::SCENARIO_COMPLETION_HOLD_FINAL_SCENE,
              &this->audit_)),
          borrowedMainNode_(0),
          runControl_("MineSweeper", diagnostics)
    {
    }

    MineSweeperStandaloneFlowAppConfig::~MineSweeperStandaloneFlowAppConfig()
    {
    }

    int MineSweeperStandaloneFlowAppConfig::exitCode() const
    {
      return this->audit_.isValid() && this->scenario_.isValid() && !this->runControl_.failed() ? 0 : 1;
    }

    void MineSweeperStandaloneFlowAppConfig::setApp(App *app)
    {
      this->runControl_.setApp(app);
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
      dsl::SnapRecord record;
      const scenario_tests::ScenarioAdvance advance =
          this->scenario_->step(this->runControl_.tick(),
                                window->scene(),
                                StandaloneContentBounds(window),
                                record);
      if (this->runControl_.observeScenarioAdvance(advance, record))
      {
        const bool replaced = this->scenario_.replace(new (std::nothrow) scenario_tests::MineSweeperScenario(
            ConfiguredMineSweeperScenarioName(),
            scenario_tests::SCENARIO_COMPLETION_HOLD_FINAL_SCENE,
            0));
        this->runControl_.completeSceneRearm(replaced && scenario_tests::RearmScenarioScene(window));
      }
    }
  } // namespace standalone_tests
} // namespace loka
