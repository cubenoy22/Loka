#include "app/bootstrap/RunApp.hpp"
#include "MyAppConfig.hpp"

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR commandLine, int show)
{
  (void)previous;
  (void)commandLine;
  return loka::platform::RunApp<SmirkyCardAppConfig>(instance, show);
}
