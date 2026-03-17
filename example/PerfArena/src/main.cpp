#include "loka/platform/Main.hpp"
#include "MyAppConfig.hpp"

#if defined(_WIN32) || defined(WIN32)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  (void)hPrevInstance;
  (void)lpCmdLine;
  return loka::platform::RunApp<MyAppConfig>(hInstance, nCmdShow);
}
#else
#error Unsupported platform for PerfArena main.cpp
#endif
