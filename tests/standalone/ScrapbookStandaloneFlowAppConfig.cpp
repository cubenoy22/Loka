#include "ScrapbookStandaloneFlowAppConfig.hpp"

#include "../scenarios/ScenarioWindow.hpp"
#include "StandaloneScenarioSupport.hpp"
#include "app/core/App.hpp"
#include "app/core/Window.hpp"

namespace loka
{
  namespace standalone_tests
  {
    ScrapbookStandaloneFlowAppConfig::ScrapbookStandaloneFlowAppConfig(PlatformContext *context,
                                                                       const platform::file::FileHandle *auditFile,
                                                                       std::FILE *diagnostics)
        : ScrapbookAppConfig(context),
          audit_(auditFile ? *auditFile : ResolveStandaloneAuditFile(), "standalone-tour"),
          scenario_(scenario_tests::ScenarioLaunchPlan::StandaloneTour(), &this->audit_),
          borrowedMainNode_(0),
          runControl_("Scrapbook", diagnostics)
    {
    }

    ScrapbookStandaloneFlowAppConfig::~ScrapbookStandaloneFlowAppConfig()
    {
      this->scenario_.stop();
    }

    int ScrapbookStandaloneFlowAppConfig::exitCode() const
    {
      return this->audit_.isValid() && !this->runControl_.failed() ? 0 : 1;
    }

    void ScrapbookStandaloneFlowAppConfig::setApp(App *app)
    {
      this->runControl_.setApp(app);
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
      const StandaloneRunControl::Advance mountAdvance = this->runControl_.advance(this->borrowedMainNode_ != 0);
      switch (mountAdvance)
      {
      case StandaloneRunControl::ADVANCE_WAITING:
      case StandaloneRunControl::ADVANCE_FAILED:
        return;
      case StandaloneRunControl::ADVANCE_MOUNTED:
        break;
      }
      if (!window)
      {
        return;
      }
      dsl::SnapRecord record;
      const scenario_tests::ScenarioAdvance advance = this->scenario_.step(this->runControl_.tick(),
                                                                           window->scene(),
                                                                           *this->borrowedMainNode_,
                                                                           StandaloneContentBounds(window),
                                                                           record);
      this->runControl_.observeScenarioAdvance(advance, record);
    }
  } // namespace standalone_tests
} // namespace loka
