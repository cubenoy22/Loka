#include "ScrapbookStandaloneFlowAppConfig.hpp"

#include "../scenarios/ScenarioWindow.hpp"
#include "StandaloneScenarioSupport.hpp"
#include "app/core/Window.hpp"

namespace loka
{
  namespace standalone_tests
  {
    ScrapbookStandaloneFlowAppConfig::ScrapbookStandaloneFlowAppConfig(PlatformContext *context)
        : ScrapbookAppConfig(context),
          audit_(ResolveStandaloneAuditFile(), "standalone-tour"),
          scenario_(scenario_tests::ScenarioLaunchPlan::StandaloneTour(), &this->audit_),
          borrowedMainNode_(0),
          tick_(0)
    {
    }

    ScrapbookStandaloneFlowAppConfig::~ScrapbookStandaloneFlowAppConfig()
    {
      this->scenario_.stop();
    }

    bool ScrapbookStandaloneFlowAppConfig::isValid() const
    {
      return this->audit_.isValid();
    }

    void ScrapbookStandaloneFlowAppConfig::compose(AppComposition &composition)
    {
      composition << scenario_tests::MakeScenarioWindow<scrapbook::MainProps, scrapbook::MainNode>(
          scrapbook::MainProps().platformContext(this->getPlatformContext()),
          &this->borrowedMainNode_,
          340,
          250,
          "Loka Scrapbook Standalone Flow",
          app::IdlePolicy::interval(0.1),
          &ScrapbookStandaloneFlowAppConfig::OnWindowIdle,
          this);
    }

    void ScrapbookStandaloneFlowAppConfig::OnWindowIdle(Window *window, double elapsedSeconds, void *userData)
    {
      (void)elapsedSeconds;
      ScrapbookStandaloneFlowAppConfig *self = static_cast<ScrapbookStandaloneFlowAppConfig *>(userData);
      if (self)
      {
        self->tick(window);
      }
    }

    void ScrapbookStandaloneFlowAppConfig::tick(Window *window)
    {
      ++this->tick_;
      if (!window || !this->borrowedMainNode_)
      {
        return;
      }
      dsl::SnapRecord record;
      const scenario_tests::ScenarioAdvance advance = this->scenario_.step(
          this->tick_, window->scene(), *this->borrowedMainNode_, StandaloneContentBounds(window), record);
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
