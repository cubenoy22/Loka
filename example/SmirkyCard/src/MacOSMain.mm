#include "app/bootstrap/RunApp.hpp"
#include "MyAppConfig.hpp"
#include <ctime>

namespace
{
  // Platform entry points deliberately share the MineSweeper injection shape.
  class ProductionAppConfig : public SmirkyCardAppConfig
  {
  public:
    explicit ProductionAppConfig(PlatformContext *context)
        : SmirkyCardAppConfig(context, smirkycard::ScriptRandom::Seeded(static_cast<unsigned long>(std::time(0))))
    {
    }
  };
} // namespace

#include <Foundation/Foundation.h>

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  int result = loka::platform::RunApp<ProductionAppConfig>();
  [pool drain];
  return result;
}
