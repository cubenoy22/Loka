#include "loka/platform/Main.hpp"
#include "MyAppConfig.hpp"
#include <Foundation/Foundation.h>

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  loka::platform::RunApp<MyAppConfig>();
  [pool release];

  return 0;
}
