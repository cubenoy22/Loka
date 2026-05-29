#include "app/bootstrap/RunApp.hpp"
#include "MyAppConfig.hpp"

#if defined(_WIN32) || defined(WIN32)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  return loka::platform::RunApp<MyAppConfig>(hInstance, nCmdShow);
}
#elif defined(LOKA_RETRO68)
int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  return loka::platform::RunApp<MyAppConfig>();
}
#else
#error Unsupported platform for MineSweeper main.cpp
#endif
