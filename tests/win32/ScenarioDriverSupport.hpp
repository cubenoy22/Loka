#ifndef LOKA_TESTS_WIN32_SCENARIO_DRIVER_SUPPORT_HPP
#define LOKA_TESTS_WIN32_SCENARIO_DRIVER_SUPPORT_HPP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cassert>

#include "SceneScenarioDriver.hpp"
#include "app/PlatformContext.hpp"
#include "app/bootstrap/PlatformBootstrap.hpp"
#include "app/core/App.hpp"
#include "core/util/ScopedPtr.hpp"
#include "testing/snap/SnapFormat.hpp"

namespace loka
{
  namespace win32_scenario_tests
  {
    enum ScenarioRunMode
    {
      SCENARIO_RUN_MODE_FLOW
    };

    /** Owns one Win32 run's audit, capture publication, completion marker,
        and host-owned linger lifecycle. Example configs supply only scenario
        advancement through the platform-neutral ScenarioDriver door. */
    class ScenarioRunState
    {
    public:
      ScenarioRunState(const dsl::SnapTestConfig::Settings &settings, ScenarioRunMode mode);
      ~ScenarioRunState();

      dsl::testing::ScenarioAuditSink *audit();
      int exitCode() const;
      void tick(Window *window, App *app, scenario_tests::ScenarioDriver &driver, double elapsedSeconds);

    private:
      class Impl;
      core::ScopedPtr<Impl> impl_;

      ScenarioRunState(const ScenarioRunState &);
      ScenarioRunState &operator=(const ScenarioRunState &);
    };

    bool LoadScenarioSettings(dsl::SnapTestConfig::Settings &settings, ScenarioRunMode &mode);
    int WriteConfigurationErrorAudit(const dsl::SnapTestConfig::Settings &settings,
                                     const char *scenario,
                                     const dsl::SnapRecord &record);

    template <class Config>
    int RunScenarioApplication(HINSTANCE hInstance,
                               int nCmdShow,
                               const dsl::SnapTestConfig::Settings &settings,
                               ScenarioRunMode mode)
    {
      platform::InitPlatformRuntime();
      core::ScopedPtr<PlatformContext> platformContext(platform::CreatePlatformContext());
      assert(platformContext.get() && "PlatformContext is required");
      if (!platformContext.get())
      {
        return 1;
      }
      Config config(platformContext.get(), settings, mode);
      if (config.exitCode() != 0)
      {
        return config.exitCode();
      }
      core::ScopedPtr<App> app(platformContext->createApp(&config, hInstance, nCmdShow));
      assert(app.get() && "App is required");
      if (!app.get())
      {
        return 1;
      }
      config.setApp(app.get());
      app->run();
      return config.exitCode();
    }
  } // namespace win32_scenario_tests
} // namespace loka

#endif // LOKA_TESTS_WIN32_SCENARIO_DRIVER_SUPPORT_HPP
