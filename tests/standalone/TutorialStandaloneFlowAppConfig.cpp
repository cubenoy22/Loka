#include "TutorialStandaloneFlowAppConfig.hpp"

#include "../scenarios/ScenarioWindow.hpp"
#include "../scenarios/ScenarioReel.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/Window.hpp"

namespace loka
{
  namespace standalone_tests
  {
    TutorialStandaloneFlowAppConfig::TutorialStandaloneFlowAppConfig(
        PlatformContext *context,
        const platform::file::FileHandle *auditFile,
        std::FILE *diagnostics)
        : AppConfigurable(context),
          audit_(auditFile ? *auditFile : ResolveStandaloneAuditFile(), "increment-summary-toggle"),
          scenario_(new (std::nothrow) scenario_tests::TutorialScenario(
              scenario_tests::SCENARIO_COMPLETION_HOLD_FINAL_SCENE, &this->audit_)),
          borrowedMainNode_(0),
          runControl_("Tutorial", diagnostics)
    {
    }

    TutorialStandaloneFlowAppConfig::~TutorialStandaloneFlowAppConfig()
    {
    }

    int TutorialStandaloneFlowAppConfig::exitCode() const
    {
      return this->audit_.isValid() && this->scenario_.isValid() && !this->runControl_.failed() ? 0 : 1;
    }

    void TutorialStandaloneFlowAppConfig::setApp(App *app)
    {
      this->runControl_.setApp(app);
    }

    void TutorialStandaloneFlowAppConfig::compose(AppComposition &composition)
    {
      composition << scenario_tests::MakeScenarioWindow<tutorial::Step4Node::PropsType, tutorial::Step4Node>(
          tutorial::Step4Node::PropsType(),
          &this->borrowedMainNode_,
          360,
          280,
          "Loka Tutorial Standalone Flow",
          app::IdlePolicy::interval(0.1),
          &TutorialStandaloneFlowAppConfig::OnWindowIdle,
          this);
    }

    void TutorialStandaloneFlowAppConfig::composeMenu(app::MenuComposition &composition)
    {
      tutorial::DeclareTutorialMenu(composition);
    }

    void TutorialStandaloneFlowAppConfig::OnWindowIdle(Window *window, double elapsedSeconds, void *userData)
    {
      (void)elapsedSeconds;
      TutorialStandaloneFlowAppConfig *self = static_cast<TutorialStandaloneFlowAppConfig *>(userData);
      if (self)
      {
        self->tick(window);
      }
    }

    void TutorialStandaloneFlowAppConfig::tick(Window *window)
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
          this->scenario_->step(this->runControl_.tick(), window->scene(), StandaloneContentBounds(window), record);
      if (this->runControl_.observeScenarioAdvance(advance, record))
      {
        const bool replaced = this->scenario_.replace(new (std::nothrow) scenario_tests::TutorialScenario(
            scenario_tests::SCENARIO_COMPLETION_HOLD_FINAL_SCENE, 0));
        this->runControl_.completeSceneRearm(replaced && scenario_tests::RearmScenarioScene(window));
      }
    }
  } // namespace standalone_tests
} // namespace loka
