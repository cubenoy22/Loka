#include "Win32Window.hpp"
#include "Win32App.hpp"
#include <windows.h>
#include <string>
#include "core/Window.hpp"

namespace
{
  static bool g_classRegistered = false;
  static const char *kWndClassName = "DevWndClass";
}

// コンストラクタを修正: Win32App* を受け取り、メンバに保存
Win32Window::Win32Window(Win32App *app, Renderer *renderer, HWND hwnd)
    : Window(renderer, app), hwnd_(hwnd), app_(app)
{
  // visibilityステートの変更を監視
  // C++98: static関数＋thisポインタ渡しでコールバック（State対応）
  visibility.bind(&Win32Window::VisibilityChangedThunk, this);
}

// static thunk for State<bool>::OnChangeFn
void Win32Window::VisibilityChangedThunk(void *userData)
{
  Win32Window *self = static_cast<Win32Window *>(userData);
  if (self)
    self->onVisibilityChanged(self->visibility.get());
}

void Win32Window::onVisibilityChanged(bool visible)
{
  if (visible)
  {
    if (!hwnd_)
    {
      createNativeWindow();
    }
    else
    {
      ShowWindow(hwnd_, SW_SHOW);
    }
  }
  else
  {
    if (hwnd_)
    {
      destroyNativeWindow();
    }
  }
}

LRESULT CALLBACK Win32Window::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  Win32Window *self = nullptr;
  if (msg == WM_NCCREATE)
  {
    CREATESTRUCT *cs = reinterpret_cast<CREATESTRUCT *>(lParam);
    self = static_cast<Win32Window *>(cs->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    self->hwnd_ = hwnd;
  }
  else
  {
    self = reinterpret_cast<Win32Window *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }

  if (self)
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
      self->hwnd_ = nullptr;
      if (self->app_)
      {                                                        // app_ポインタが有効か確認
        self->app_->windowClosed(static_cast<Window *>(self)); // 明示的にWindowポインタにキャスト
      }
      break;
    default:
      return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

void Win32Window::createNativeWindow()
{
  if (!g_classRegistered)
  {
    WNDCLASSA wc = {};
    wc.lpfnWndProc = Win32Window::WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = kWndClassName;
    RegisterClassA(&wc);
    g_classRegistered = true;
  }
  HWND hwnd = CreateWindowA(
      kWndClassName,
      "Developer",
      WS_OVERLAPPEDWINDOW,
      100, 100, 320, 200,
      nullptr, nullptr, GetModuleHandle(nullptr),
      this); // lParamにthis (Win32Window*) を渡す
  if (hwnd)  // hwnd_ではなくローカル変数hwndでチェック
  {
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
  }
}

void Win32Window::destroyNativeWindow()
{
  if (hwnd_)
  {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
}
