#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "RetirementProbeSampler.hpp"
#include "MainNode.hpp"
#include "ScrapbookSceneIds.hpp"
#include "ScenarioWindow.hpp"
#include "app/bootstrap/PlatformBootstrap.hpp"
#include "app/core/App.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/core/Window.hpp"
#include "core/util/ScopedPtr.hpp"
#include "platform/StringUTF8.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace
{
  class ScrapbookProbe : public AppConfigurable
  {
  public:
    explicit ScrapbookProbe(PlatformContext *context)
        : AppConfigurable(context), log_(L"retirement-probe-scrapbook.log"), app_(0),
          main_(0), phase_(WAIT), step_(0) {}
    bool ready() const { return this->log_.valid(); }
    int exitCode() const { return this->phase_ == DONE && this->log_.valid() ? 0 : 1; }
    void setApp(App *app) { this->app_ = app; }
    virtual void compose(AppComposition &composition)
    {
      composition << loka::scenario_tests::MakeScenarioWindow<scrapbook::MainProps, scrapbook::MainNode>(
          scrapbook::MainProps().platformContext(this->getPlatformContext()), &this->main_,
          340, 274, "Scrapbook retirement probe", loka::app::IdlePolicy::interval(0.05), &OnIdle, this);
    }

  private:
    enum Phase { WAIT, NAV, NAV_SETTLE, BURST, RESET, BURST_SETTLE, DONE };
    static void OnIdle(Window *window, double, void *data)
    {
      static_cast<ScrapbookProbe *>(data)->tick(window);
    }
    void fail(const char *reason)
    {
      this->log_.error(reason);
      this->app_->quit();
    }
    void sample(const char *point)
    {
      int page = -1;
      if (this->main_) this->main_->queryCurrentPageIndex(page);
      // MainNode exposes the published page and caption, but no Image handle.
      this->log_.sample(*this->getPlatformContext(), this->step_, point, page + 1);
    }
    bool click(Window *window, bool next)
    {
      if (!window || !window->scene())
      {
        this->fail("missing-scene");
        return false;
      }
      int before = -1;
      if (!this->main_ || !this->main_->queryCurrentPageIndex(before))
      {
        this->fail("missing-page");
        return false;
      }
      loka::app::scene::Scene *scene = window->scene();
      loka::app::scene::Scene *out = 0;
      loka::dsl::FlowError error;
      if (loka::dsl::testing::ClickButton(next ? scrapbook::scene_ids::NextButton()
                                              : scrapbook::scene_ids::PreviousButton()).run(scene, out, error)
          != loka::dsl::FLOW_STEP_SUCCEEDED)
      {
        this->fail("click-failed");
        return false;
      }
      int after = -1;
      if (!this->main_->queryCurrentPageIndex(after) || after != before + (next ? 1 : -1))
      {
        this->fail("page-did-not-change");
        return false;
      }
      return true;
    }
    void tick(Window *window)
    {
      if (!this->log_.valid()) { this->app_->quit(); return; }
      switch (this->phase_)
      {
      case WAIT:
      {
        std::string caption;
        if (this->main_ && loka::platform::CollectUtf8(this->main_->displayedCaption(), caption)
            && caption == "1 / 5")
        {
          this->phase_ = NAV;
          this->step_ = 0;
          this->log_.begin("nav");
        }
        else if (++this->step_ >= 100) this->fail("mount-timeout");
        return;
      }
      case NAV:
        this->sample("pre");
        if (!this->click(window, this->step_ % 8 < 4)) return;
        this->sample("post");
        if (++this->step_ == 40) { this->phase_ = NAV_SETTLE; this->step_ = 0; }
        return;
      case NAV_SETTLE:
      case BURST_SETTLE:
        this->sample("settle");
        if (++this->step_ == 5)
        {
          this->log_.summary();
          if (this->phase_ == BURST_SETTLE)
          {
            this->phase_ = DONE;
            this->app_->quit();
          }
          else
          {
            this->phase_ = BURST;
            this->step_ = 0;
            this->log_.begin("burst");
          }
        }
        return;
      case BURST:
        this->sample("pre");
        if (!this->click(window, true)) return;
        this->sample("post1");
        if (!this->click(window, true)) return;
        this->sample("post2");
        if (!this->click(window, false)) return;
        this->sample("post3");
        if (++this->step_ == 10) { this->phase_ = BURST_SETTLE; this->step_ = 0; }
        else this->phase_ = RESET;
        return;
      case RESET:
        this->sample("reset-pre");
        if (!this->click(window, false)) return;
        this->sample("reset-post");
        this->phase_ = BURST;
        return;
      case DONE:
        return;
      }
    }
    RetirementProbeLog log_;
    App *app_;
    scrapbook::MainNode *main_;
    Phase phase_;
    int step_;
  };
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
  loka::platform::InitPlatformRuntime();
  loka::core::ScopedPtr<PlatformContext> context(loka::platform::CreatePlatformContext());
  if (!context.get()) return 1;
  ScrapbookProbe config(context.get());
  if (!config.ready()) return 1;
  loka::core::ScopedPtr<App> app(context->createApp(&config, instance, show));
  if (!app.get()) return 1;
  config.setApp(app.get());
  app->run();
  return config.exitCode();
}
