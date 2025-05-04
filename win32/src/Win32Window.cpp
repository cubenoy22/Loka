#include "Win32Window.hpp"
#include <windows.h>
#include <string>

namespace
{
  static bool g_classRegistered = false;
  static const char *kWndClassName = "DevWndClass";
}

Win32Window::Win32Window(Renderer *renderer, HWND hwnd)
    : Window(renderer), hwnd_(hwnd)
{
  // visibilityプロパティの変更を監視
  // if (visibility->get())
  // {
  //   // C++98: static関数＋thisポインタ渡しでコールバック
  //   visibility->bind(&Win32Window::VisibilityChangedThunk, this);
  // }
}

// static thunk for BindableProp<bool>::OnChangeFn
void Win32Window::VisibilityChangedThunk(bool visible, void *userData)
{
  Win32Window *self = static_cast<Win32Window *>(userData);
  if (self)
    self->onVisibilityChanged(visible);
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
      nullptr, nullptr, GetModuleHandle(nullptr), this);
  hwnd_ = hwnd;
  if (hwnd_)
  {
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
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
