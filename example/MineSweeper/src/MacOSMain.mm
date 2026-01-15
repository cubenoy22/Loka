#include "loka/platform/Main.hpp"
#include "MyAppConfig.hpp"

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  @autoreleasepool {
    loka::platform::RunApp<MyAppConfig>();
  }

  return 0;
}
