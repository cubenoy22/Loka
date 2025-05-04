#include "Win32App.hpp"
#include <windows.h>

Win32App::Win32App(HINSTANCE hInstance, int nCmdShow)
    : App(nullptr), hInstance_(hInstance), nCmdShow_(nCmdShow), mainWindow_(nullptr)
{
  mainWindow_ = new Win32Window(nullptr);
  // App基底クラスのwindowにmainWindow_をセット
  // Appの設計上、windowはprivateなので、コンストラクタで渡す形にする
  // ここではApp(mainWindow_)にしたいが、C++の委譲コンストラクタが使えない場合は設計見直しも検討
}

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
