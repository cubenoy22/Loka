#include "Win32Window.hpp"
#include "Win32App.hpp"
#include <windows.h>
#include <string>
#include "core/Window.hpp"
#include "core/StateTracker.hpp"

namespace
{
  static bool g_classRegistered = false;
  static const char *kWndClassName = "DevWndClass";
}

Win32Window::Win32Window(Win32App *app, PlatformContext *context, const std::string &title, HWND hwnd)
    : Window(context, app, title), hwnd_(hwnd), app_(app)
{
  // visibilityステートの変更を監視
  this->visibility.deferBind(&Win32Window::VisibilityChangedThunk, this);
  // Window::title自体の変更も監視
  this->title.deferBind(&Win32Window::TitleChangedThunk, this);
}

// static thunk for State<bool>::OnChangeFn
void Win32Window::VisibilityChangedThunk(void *userData)
{
  Win32Window *self = static_cast<Win32Window *>(userData);
  if (!self)
    return;
  bool visible = self->visibility.get();
  if (visible)
  {
    if (!self->hwnd_)
    {
      self->createNativeWindow();
    }
    else
    {
      ShowWindow(self->hwnd_, SW_SHOW);
    }
  }
  else
  {
    if (self->hwnd_)
    {
      self->destroyNativeWindow();
    }
  }
}

// --- タイトル変更時のthunk ---
void Win32Window::TitleChangedThunk(void *userData)
{
  Win32Window *self = static_cast<Win32Window *>(userData);
  if (self && self->hwnd_)
  {
    // Window基底のtitle値をウィンドウに反映
    SetWindowTextA(self->hwnd_, self->title.get().c_str());
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
    case WM_COMMAND:
      if (LOWORD(wParam) == 1001 && HIWORD(wParam) == BN_CLICKED)
      {
        if (self->getTracker())
        {
          self->getTracker()->begin();
          self->title.set("CLICKED!");
          self->getTracker()->end();
        }
      }
      break;
    case WM_PAINT:
    {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      // 背景を白で塗りつぶす
      RECT rc;
      GetClientRect(hwnd, &rc);
      FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));
      // 既存の描画
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
  // --- タイトルをWindow::titleから取得 ---
  HWND hwnd = CreateWindowA(
      kWndClassName,
      this->title.get().c_str(),
      WS_OVERLAPPEDWINDOW,
      100, 100, 320, 200,
      nullptr, nullptr, GetModuleHandle(nullptr),
      this);
  if (hwnd)
  {
    this->hwnd_ = hwnd;
    // --- 追加: ボタン生成 ---
    this->buttonHwnd_ = CreateWindowA(
        "BUTTON",
        "Click me!",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        50, 120, 200, 40, // x, y, width, height
        this->hwnd_,
        (HMENU)1001, // コントロールID
        GetModuleHandle(nullptr),
        NULL);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
  }
}

void Win32Window::destroyNativeWindow()
{
  if (this->hwnd_)
  {
    DestroyWindow(this->hwnd_);
    this->hwnd_ = nullptr;
  }
}
