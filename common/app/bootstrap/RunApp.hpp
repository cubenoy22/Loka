#ifndef LOKA_APP_BOOTSTRAP_RUNAPP_HPP
#define LOKA_APP_BOOTSTRAP_RUNAPP_HPP

#include <cassert>
#include "app/core/App.hpp"
#include "app/PlatformContext.hpp"
#include "app/bootstrap/PlatformBootstrap.hpp"
#include "core/util/ScopedPtr.hpp"

namespace loka
{
  namespace platform
  {
    template <class ConfigT> int RunApp(HINSTANCE hInstance, int nCmdShow)
    {
      InitPlatformRuntime();
      loka::core::ScopedPtr<PlatformContext> platformContext(CreatePlatformContext());
      assert(platformContext.get() && "PlatformContext is required");
      ConfigT config(platformContext.get());
      loka::core::ScopedPtr<App> app(platformContext->createApp(&config, hInstance, nCmdShow));
      app->run();
      return 0;
    }

    template <class ConfigT> int RunApp()
    {
      return RunApp<ConfigT>(0, 0);
    }

  } // namespace platform
} // namespace loka

#endif // LOKA_APP_BOOTSTRAP_RUNAPP_HPP
