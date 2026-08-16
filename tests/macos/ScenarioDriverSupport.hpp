#ifndef LOKA_TESTS_MACOS_SCENARIO_DRIVER_SUPPORT_HPP
#define LOKA_TESTS_MACOS_SCENARIO_DRIVER_SUPPORT_HPP

#include <cassert>

#include "ScenarioTypes.hpp"
#include "StartupScenarios.hpp"
#include "app/PlatformContext.hpp"
#include "app/bootstrap/PlatformBootstrap.hpp"
#include "app/core/App.hpp"
#include "app/core/Window.hpp"
#include "core/util/ScopedPtr.hpp"
#include "testing/scene/ScenarioAudit.hpp"

namespace loka
{
  namespace macos_scenario_tests
  {
    enum ScenarioRunMode
    {
      SCENARIO_RUN_MODE_FLOW,
      SCENARIO_RUN_MODE_INSPECT
    };

    /** One example-specific scenario adapter driven by the shared macOS
        capture, settle, and artifact owner. */
    class ScenarioDriver
    {
    public:
      virtual ~ScenarioDriver();

      virtual scenario_tests::ScenarioAdvance
      step(long tick, Window *window, const scenario_tests::CaptureContentBounds &bounds, dsl::SnapRecord &out) = 0;
      virtual bool publishVerdict(const dsl::SnapRecord &record) = 0;
    };

    /** Owns one macOS run's audit and capture lifecycle. Example drivers only
        supply scenario advancement; artifact publication stays one rail. */
    class ScenarioRunState
    {
    public:
      ScenarioRunState(const dsl::SnapTestConfig::Settings &settings, ScenarioRunMode mode);
      ~ScenarioRunState();

      dsl::testing::ScenarioAuditSink *audit();
      void tick(Window *window, App *app, ScenarioDriver &driver);

    private:
      class Impl;
      core::ScopedPtr<Impl> impl_;

      ScenarioRunState(const ScenarioRunState &);
      ScenarioRunState &operator=(const ScenarioRunState &);
    };

    typedef dsl::SnapRecord (*DriverErrorFactory)(long errorCode, const char *message);

    /** Shared L1/L2 adapter for scene-only examples. All four startup cells
        therefore use the same startup scenario and completion path. */
    template <class InteractionScenario> class SceneScenarioDriver : public ScenarioDriver
    {
    public:
      SceneScenarioDriver(bool startup,
                          scenario_tests::StartupExample startupExample,
                          DriverErrorFactory errorFactory,
                          long interactionErrorCode,
                          dsl::testing::ScenarioAuditSink *audit)
          : startup_(startup),
            startupExample_(startupExample),
            errorFactory_(errorFactory),
            interactionErrorCode_(interactionErrorCode),
            startupScenario_(startupExample, scenario_tests::SCENARIO_COMPLETION_DRIVER_OWNED, audit),
            scenario_(scenario_tests::SCENARIO_COMPLETION_DRIVER_OWNED, audit)
      {
      }

      virtual ~SceneScenarioDriver()
      {
        if (this->startup_)
        {
          this->startupScenario_.stop();
        }
        else
        {
          this->scenario_.stop();
        }
      }

      virtual scenario_tests::ScenarioAdvance
      step(long tick, Window *window, const scenario_tests::CaptureContentBounds &bounds, dsl::SnapRecord &out)
      {
        if (!window || !window->scene())
        {
          out = this->startup_
                    ? scenario_tests::MakeStartupDriverErrorRecord(this->startupExample_, 2802, "Scene was not mounted")
                    : this->errorFactory_(this->interactionErrorCode_, "Scene was not mounted");
          return scenario_tests::SCENARIO_ADVANCE_DRIVER_COMPLETION_READY;
        }
        return this->startup_ ? this->startupScenario_.step(tick, window->scene(), bounds, out)
                              : this->scenario_.step(tick, window->scene(), bounds, out);
      }

      virtual bool publishVerdict(const dsl::SnapRecord &record)
      {
        return this->startup_ ? this->startupScenario_.publishVerdict(record) : this->scenario_.publishVerdict(record);
      }

    private:
      const bool startup_;
      const scenario_tests::StartupExample startupExample_;
      DriverErrorFactory errorFactory_;
      const long interactionErrorCode_;
      scenario_tests::StartupScenario startupScenario_;
      InteractionScenario scenario_;

      SceneScenarioDriver(const SceneScenarioDriver &);
      SceneScenarioDriver &operator=(const SceneScenarioDriver &);
    };

    bool LoadScenarioSettings(dsl::SnapTestConfig::Settings &settings, ScenarioRunMode &mode);

    template <class Config>
    int RunScenarioApplication(PlatformContext *context,
                               const dsl::SnapTestConfig::Settings &settings,
                               ScenarioRunMode mode)
    {
      Config config(context, settings, mode);
      core::ScopedPtr<App> app(context->createApp(&config, 0, 0));
      assert(app.get() && "App is required");
      if (!app.get())
      {
        return 1;
      }
      config.setApp(app.get());
      app->run();
      return 0;
    }

    template <class Config>
    int RunScenarioApplication(const dsl::SnapTestConfig::Settings &settings, ScenarioRunMode mode)
    {
      platform::InitPlatformRuntime();
      core::ScopedPtr<PlatformContext> platformContext(platform::CreatePlatformContext());
      assert(platformContext.get() && "PlatformContext is required");
      if (!platformContext.get())
      {
        return 1;
      }
      return RunScenarioApplication<Config>(platformContext.get(), settings, mode);
    }

    typedef int (*ScenarioApplicationMain)();
    int RunScenarioMain(ScenarioApplicationMain applicationMain);
  } // namespace macos_scenario_tests
} // namespace loka

#endif // LOKA_TESTS_MACOS_SCENARIO_DRIVER_SUPPORT_HPP
