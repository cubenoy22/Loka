#include "Win32App.hpp"
#include <windows.h>

Win32App::Win32App(HINSTANCE hInstance, int nCmdShow)
    : App(nullptr), hInstance_(hInstance), nCmdShow_(nCmdShow) {}

void Win32App::run()
{
  // ここでWin32のメッセージループを実装
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}
