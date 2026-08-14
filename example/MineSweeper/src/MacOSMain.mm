#include "app/bootstrap/RunApp.hpp"
#include "MyAppConfig.hpp"
#include <Foundation/Foundation.h>
#include <ctime>

namespace
{
  class ProductionAppConfig : public MyAppConfig
  {
  public:
    explicit ProductionAppConfig(PlatformContext *context)
        : MyAppConfig(
              context,
              minesweeper::MainProps(
                  static_cast<unsigned long>(std::time(0))))
    {
    }
  };
}

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  int result = loka::platform::RunApp<ProductionAppConfig>();
  // Leopard/PPC may crash while popping the outermost process-lifetime pool
  // during shutdown. The process is exiting here anyway, so let the OS reclaim
  // it instead of releasing the pool explicitly.
  (void)pool;
  return result;
}
