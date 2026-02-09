#include "loka/platform/Main.hpp"
#include "MyAppConfig.hpp"
#include <Foundation/Foundation.h>

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

#if defined(__has_feature)
#if __has_feature(objc_arc)
  @autoreleasepool {
    loka::platform::RunApp<MyAppConfig>();
  }
#else
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  loka::platform::RunApp<MyAppConfig>();
  [pool release];
#endif
#else
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  loka::platform::RunApp<MyAppConfig>();
  [pool release];
#endif

  return 0;
}
