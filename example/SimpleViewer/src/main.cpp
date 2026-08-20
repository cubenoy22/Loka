#include "app/bootstrap/RunApp.hpp"
#include "MyAppConfig.hpp"

#if defined(_WIN32) || defined(WIN32)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  (void)hPrevInstance;
  (void)lpCmdLine;
  return loka::platform::RunApp<SimpleViewerAppConfig>(hInstance, nCmdShow);
}
#elif defined(LOKA_RETRO68)
int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  return loka::platform::RunApp<SimpleViewerAppConfig>();
}
#else
#error Unsupported platform for SimpleViewer main.cpp
#endif
