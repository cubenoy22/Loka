#include "ToolboxPlatformContext.hpp"
#include "core/App.hpp"
#include "core/util/ScopedPtr.hpp"
#include "MyAppConfig.hpp"

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  ToolboxPlatformContext platformContext;
  MyAppConfig config(&platformContext);
  ScopedPtr<App>(platformContext.createApp(&config, 0, 0))
      ->run();

  return 0;
}
