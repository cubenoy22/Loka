#include <cstdio>
#include <Foundation/Foundation.h>
#include "RetirementProbeLazy.hpp"

int main()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  (void)pool;
  loka::platform::InitPlatformRuntime();
  loka::core::ScopedPtr<PlatformContext> context(loka::platform::CreatePlatformContext());
  if (!context.get()) { std::fprintf(stderr, "error=platform-context-create-failed\n"); return 1; }
  LazyProbe config(context.get());
  if (!config.ready()) return 1;
  loka::core::ScopedPtr<App> app(context->createApp(&config, 0, 0));
  if (!app.get()) { std::fprintf(stderr, "error=app-create-failed\n"); return 1; }
  config.setApp(app.get());
  app->run();
  return config.exitCode();
}
