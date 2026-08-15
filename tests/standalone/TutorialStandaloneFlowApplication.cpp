#include "TutorialStandaloneFlowApplication.hpp"

#include <cassert>

#include "TutorialStandaloneFlowAppConfig.hpp"
#include "app/bootstrap/PlatformBootstrap.hpp"
#include "app/core/App.hpp"
#include "core/util/ScopedPtr.hpp"

namespace loka
{
  namespace standalone_tests
  {
    int RunTutorialStandaloneFlowApplication(HINSTANCE hInstance, int nCmdShow)
    {
      platform::InitPlatformRuntime();
      core::ScopedPtr<PlatformContext> platformContext(platform::CreatePlatformContext());
      assert(platformContext.get() && "PlatformContext is required");
      if (!platformContext.get())
      {
        return 1;
      }
      TutorialStandaloneFlowAppConfig config(platformContext.get());
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
  } // namespace standalone_tests
} // namespace loka
