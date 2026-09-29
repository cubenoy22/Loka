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

#include <windows.h>

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR commandLine, int show)
{
  (void)previous;
  (void)commandLine;
  return loka::platform::RunApp<ProductionAppConfig>(instance, show);
}
