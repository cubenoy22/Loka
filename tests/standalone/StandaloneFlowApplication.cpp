#include "StandaloneFlowApplication.hpp"

#include <cassert>

#include "ScrapbookStandaloneFlowAppConfig.hpp"
#include "app/PlatformContext.hpp"
#include "app/bootstrap/PlatformBootstrap.hpp"
#include "app/core/App.hpp"
#include "core/util/ScopedPtr.hpp"

namespace loka
{
  namespace standalone_tests
  {
    int RunStandaloneFlowApplication(HINSTANCE hInstance, int nCmdShow)
    {
      platform::InitPlatformRuntime();
      core::ScopedPtr<PlatformContext> platformContext(platform::CreatePlatformContext());
      assert(platformContext.get() && "PlatformContext is required");
      if (!platformContext.get())
      {
        return 1;
      }
      ScrapbookStandaloneFlowAppConfig config(platformContext.get());
      if (!config.isValid())
      {
        return 1;
      }
      core::ScopedPtr<App> app(platformContext->createApp(&config, hInstance, nCmdShow));
      assert(app.get() && "App is required");
      if (!app.get())
      {
        return 1;
      }
      app->run();
      return 0;
    }
  } // namespace standalone_tests
} // namespace loka
