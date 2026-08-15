#include "app/bootstrap/RunApp.hpp"
#include "ProductionAppConfig.hpp"

#if defined(_WIN32) || defined(WIN32)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  (void)hPrevInstance;
  (void)lpCmdLine;
  return loka::platform::RunApp<HelloWorldProductionAppConfig>(hInstance, nCmdShow);
}
#elif defined(LOKA_RETRO68)
int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  return loka::platform::RunApp<HelloWorldProductionAppConfig>();
}
#else
#error Unsupported platform for HelloWorld main.cpp
#endif
