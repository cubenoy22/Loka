#include <windows.h>
#include "Win32App.hpp"

// --- Win32Window/Win32App設計に基づき、WinMainを最小構成に整理 ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  Win32App app(hInstance, nCmdShow);
  app.run();
  return 0;
}
