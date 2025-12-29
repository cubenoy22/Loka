#include "MacPlatformContext.hpp"
#include "core/App.hpp"
#include "core/util/ScopedPtr.hpp"
#include "MyAppConfig.hpp"

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  @autoreleasepool {
    MacPlatformContext platformContext;
    MyAppConfig config(&platformContext);
    ScopedPtr<App>(platformContext.createApp(&config, 0, 0))
        ->run();
  }

  return 0;
}
