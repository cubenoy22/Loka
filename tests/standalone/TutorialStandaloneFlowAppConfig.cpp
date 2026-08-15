#include "TutorialStandaloneFlowAppConfig.hpp"

#include "../scenarios/ScenarioWindow.hpp"
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
          scenario_(scenario_tests::SCENARIO_COMPLETION_HOLD_FINAL_SCENE, &this->audit_),
          borrowedMainNode_(0),
          mountDeadline_("Tutorial", diagnostics)
    {
    }

    TutorialStandaloneFlowAppConfig::~TutorialStandaloneFlowAppConfig()
    {
      this->scenario_.stop();
    }

    int TutorialStandaloneFlowAppConfig::exitCode() const
    {
      return this->audit_.isValid() && !this->mountDeadline_.failed() ? 0 : 1;
    }

    void TutorialStandaloneFlowAppConfig::setApp(App *app)
    {
      this->mountDeadline_.setApp(app);
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
          this->scenario_.step(this->mountDeadline_.tick(), window->scene(), StandaloneContentBounds(window), record);
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
