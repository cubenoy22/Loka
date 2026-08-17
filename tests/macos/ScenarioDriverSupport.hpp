#ifndef LOKA_TESTS_MACOS_SCENARIO_DRIVER_SUPPORT_HPP
#define LOKA_TESTS_MACOS_SCENARIO_DRIVER_SUPPORT_HPP

#include <cassert>

#include "SceneScenarioDriver.hpp"
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

    /** Owns one macOS run's audit and capture lifecycle. Example drivers only
        supply scenario advancement; artifact publication stays one rail. */
    class ScenarioRunState
    {
    public:
      ScenarioRunState(const dsl::SnapTestConfig::Settings &settings, ScenarioRunMode mode);
      ~ScenarioRunState();

      dsl::testing::ScenarioAuditSink *audit();
      void tick(Window *window, App *app, scenario_tests::ScenarioDriver &driver);

    private:
      class Impl;
      core::ScopedPtr<Impl> impl_;

      ScenarioRunState(const ScenarioRunState &);
      ScenarioRunState &operator=(const ScenarioRunState &);
    };

    bool LoadScenarioSettings(dsl::SnapTestConfig::Settings &settings, ScenarioRunMode &mode);

    template <class Config>
    int RunScenarioApplication(PlatformContext *context, Config &config)
    {
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
    int RunScenarioApplication(PlatformContext *context,
                               const dsl::SnapTestConfig::Settings &settings,
                               ScenarioRunMode mode)
    {
      Config config(context, settings, mode);
      return RunScenarioApplication(context, config);
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

    /** Runs a presentation config without loading the macOS host scenario
        protocol. Loop reels use this path so they can be launched by a person
        and never wait for config, capture, or completion-marker traffic. */
    template <class Config>
    int RunScenarioApplication()
    {
      platform::InitPlatformRuntime();
      core::ScopedPtr<PlatformContext> platformContext(platform::CreatePlatformContext());
      assert(platformContext.get() && "PlatformContext is required");
      if (!platformContext.get())
      {
        return 1;
      }
      Config config(platformContext.get());
      return RunScenarioApplication(platformContext.get(), config);
    }

    typedef int (*ScenarioApplicationMain)();
    int RunScenarioMain(ScenarioApplicationMain applicationMain);
  } // namespace macos_scenario_tests
} // namespace loka

#endif // LOKA_TESTS_MACOS_SCENARIO_DRIVER_SUPPORT_HPP
