#include <windows.h>
#include "Win32App.hpp"
#include "Win32Window.hpp"

// SampleWindowクラスの定義・利用例は削除（Win32Windowのみで設計を進める）

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
  case WM_PAINT:
  {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    TextOutA(hdc, 20, 20, "Developer", 9);
    EndPaint(hwnd, &ps);
    break;
  }
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  default:
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }
  return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  WNDCLASSA wc = {0};
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hInstance;
  wc.lpszClassName = "DevWndClass";
  RegisterClassA(&wc);
  HWND hwnd = CreateWindowA("DevWndClass", "Developer", WS_OVERLAPPEDWINDOW, 100, 100, 320, 200, NULL, NULL, hInstance, NULL);
  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);

  Win32App app(hInstance, nCmdShow);
  app.run();
  return 0;
}

// --- 既存のWinMainは残しつつ、新設計例もWin32Windowのみでコメントアウト ---
/*
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // --- 新設計例 ---
    // 1. 必要ならウィンドウクラス登録（Win32Window::registerClass）
    // 2. Win32Windowインスタンス生成
    Win32Window win(hInstance, nCmdShow, "DevWndClass", "Developer");
    // 3. 必要ならPageやRendererのセット
    // win.setPage(...);
    // 4. ウィンドウ表示・メッセージループ
    win.show();
    win.run();
    return 0;
}
*/
