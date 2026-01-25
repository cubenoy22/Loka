#ifndef LOKA_PLATFORM_MAIN_HPP
#define LOKA_PLATFORM_MAIN_HPP

#include <cassert>
#include "app/App.hpp"
#include "app/PlatformContext.hpp"
#include "core/util/ScopedPtr.hpp"

#if defined(_WIN32) || defined(WIN32)
#include "Win32PlatformContext.hpp"
#elif defined(LOKA_RETRO68)
#include "ToolboxPlatformContext.hpp"
#include "ToolboxProfiler.hpp"
#elif defined(__APPLE__)
#include "MacPlatformContext.hpp"
#endif

namespace loka
{
  namespace platform
  {
    inline PlatformContext *CreatePlatformContext()
    {
#if defined(_WIN32) || defined(WIN32)
      return new Win32PlatformContext();
#elif defined(LOKA_RETRO68)
      return new ToolboxPlatformContext();
#elif defined(__APPLE__)
      return new MacPlatformContext();
#else
      return 0;
#endif
    }

    template <class ConfigT>
    int RunApp(HINSTANCE hInstance, int nCmdShow)
    {
#if defined(LOKA_RETRO68)
      InitToolboxProfiler();
#endif
      loka::core::ScopedPtr<PlatformContext> platformContext(CreatePlatformContext());
      assert(platformContext.get() && "PlatformContext is required");
      ConfigT config(platformContext.get());
      loka::core::ScopedPtr<App> app(platformContext->createApp(&config, hInstance, nCmdShow));
      app->run();
      return 0;
    }

    template <class ConfigT>
    int RunApp()
    {
      return RunApp<ConfigT>(0, 0);
    }

  } // namespace platform
} // namespace loka

#endif // LOKA_PLATFORM_MAIN_HPP
