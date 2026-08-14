#include "app/bootstrap/RunApp.hpp"
#include "MyAppConfig.hpp"
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

#if defined(_WIN32) || defined(WIN32)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  (void)hPrevInstance;
  (void)lpCmdLine;
  return loka::platform::RunApp<ProductionAppConfig>(hInstance, nCmdShow);
}
#elif defined(LOKA_RETRO68)
int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  return loka::platform::RunApp<ProductionAppConfig>();
}
#else
#error Unsupported platform for MineSweeper main.cpp
#endif
