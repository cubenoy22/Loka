#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "RetirementProbeScrapbook.hpp"

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
