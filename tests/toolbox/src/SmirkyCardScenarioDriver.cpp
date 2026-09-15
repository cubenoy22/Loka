#include "SmirkyCardScenarioDriver.hpp"

#include <cassert>
#include "MyAppConfig.hpp"
#include "ScenarioDriverSupport.hpp"
#include "SmirkyCardScenarios.hpp"
#include "app/PlatformContext.hpp"
#include "app/bootstrap/PlatformBootstrap.hpp"
#include "app/core/App.hpp"
#include "core/util/ScopedPtr.hpp"
#include "testing/scene/ScenarioAudit.hpp"

namespace loka { namespace toolbox_tests {
namespace {
const char *kConfigPath = "LokaTest.cfg";
const char *kDefaultScenarioName = "run-javascript";
class SmirkyCardScenarioAppConfig : public ::SmirkyCardAppConfig {
public:
  SmirkyCardScenarioAppConfig(PlatformContext *context, const dsl::SnapTestConfig::Settings &settings)
      : ::SmirkyCardAppConfig(context), audit_(ResolveScenarioAuditFile(), settings.scenario.c_str()),
        scenario_(settings.scenario, scenario_tests::SCENARIO_COMPLETION_DRIVER_OWNED, &audit_),
        app_(0), recorded_(false), tick_(0), linger_(settings.hasLingerSeconds ? settings.lingerSeconds : 0), completion_() {}
  virtual ~SmirkyCardScenarioAppConfig() { this->scenario_.stop(); }
  void setApp(App *app) { this->app_ = app; }
protected:
  virtual void onWindowIdle(Window *window, double elapsedSeconds) {
    ++this->tick_;
    if (!this->recorded_) {
      dsl::SnapRecord record;
      if (!window || !window->scene()) {
        record = scenario_tests::MakeSmirkyCardDriverErrorRecord(2904, "Scene was not mounted");
        this->recorded_ = true;
      } else if (this->scenario_.step(this->tick_, window->scene(),
                                      ContentLocalBounds(QueryCaptureContentBounds(window)), record)
                 == scenario_tests::SCENARIO_ADVANCE_DRIVER_COMPLETION_READY) {
        this->recorded_ = true;
      }
      if (this->recorded_) { (void)this->scenario_.publishVerdict(record); (void)this->completion_.publish(window); }
    }
    if (this->recorded_) { this->linger_ -= elapsedSeconds; if (this->linger_ <= 0 && this->app_) this->app_->quit(); }
  }
private:
  dsl::testing::ScenarioAuditFile audit_; scenario_tests::SmirkyCardScenario scenario_; App *app_;
  bool recorded_; long tick_; double linger_; ScenarioCompletionPublisher completion_;
};
}
int RunSmirkyCardScenarioApplication() {
  dsl::SnapTestConfig::Settings settings;
  if (!dsl::SnapTestConfig::load(kConfigPath, settings) || !settings.hasScenario || !scenario_tests::IsSmirkyCardScenario(settings.scenario)) {
    (void)WriteScenarioErrorAudit(kDefaultScenarioName, scenario_tests::MakeSmirkyCardDriverErrorRecord(2900, "scenario is missing or not registered")); return 0;
  }
  platform::InitPlatformRuntime(); core::ScopedPtr<PlatformContext> context(platform::CreatePlatformContext());
  assert(context.get()); if (!context.get()) return 1;
  SmirkyCardScenarioAppConfig config(context.get(), settings); core::ScopedPtr<App> app(context->createApp(&config, 0, 0));
  assert(app.get()); if (!app.get()) return 1; config.setApp(app.get()); app->run(); return 0;
}
} }
