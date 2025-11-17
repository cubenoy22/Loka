#include "Win32Window.hpp"
#include "Win32App.hpp"
#include <windows.h>
#include <string>
#include "core/Window.hpp"
#include "core/StateTracker.hpp"

#include "core2/scene/Scene.hpp"
#include "core2/scene/NodeManager.hpp"
#include "core/util/AutoTransactionGuard.hpp"
#include "Win32ScenePlatformController.hpp"
#include "core/strings/Strings.hpp"

namespace
{
  static bool g_classRegistered = false;
  static const char *kWndClassName = "DevWndClass";
}

Win32Window::Win32Window(PlatformContext *context, declara::core::scene::Scene *initialScene, const WindowOptions &opts)
    : Window(context, initialScene, opts), hwnd_(NULL), app_(NULL), nodeManager_(0), scenePlatformController_(0)
{
  // visibilityステートの変更を監視
  this->visibility.deferBind(&Win32Window::VisibilityChangedThunk, this);
  this->title.deferBind(&Win32Window::TitleChangedThunk, this);
}

Win32Window::~Win32Window()
{
  teardownScene();
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
    if (self->hwnd_)
      self->onShow();
  }
  else
  {
    if (self->hwnd_)
    {
      self->onHide(); // 非表示直前にonHideを呼ぶ
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
    // Window基底のtitle値(UTF-8)をUTF-16に変換して反映
    const std::string &t = self->title.get();
    std::wstring w;
    if (declara::core::strings::utf8_to_wstring(t, w))
      SetWindowTextW(self->hwnd_, w.c_str());
    else
      SetWindowTextA(self->hwnd_, t.c_str());
  }
}

LRESULT CALLBACK Win32Window::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  Win32Window *self = NULL;
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
      if (self->handleCommand(wParam, lParam))
      {
        return 0;
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
      // --- onDestroyイベントをここで一元的に呼ぶ ---
      self->onDestroy();
      self->hwnd_ = NULL;
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
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = Win32Window::WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = kWndClassName;
    RegisterClassA(&wc);
    g_classRegistered = true;
  }
  HWND hwnd = CreateWindowA(
      kWndClassName,
      this->title.get().c_str(),
      WS_OVERLAPPEDWINDOW,
      100, 100, 320, 200,
      NULL, NULL, GetModuleHandle(NULL),
      this);
  if (hwnd)
  {
    this->hwnd_ = hwnd;
    UpdateWindow(hwnd);
    this->onCreate();
    this->mountScene();
  }
}

void Win32Window::destroyNativeWindow()
{
  if (this->hwnd_)
  {
    teardownScene();
    DestroyWindow(this->hwnd_);
    this->hwnd_ = NULL;
  }
}

void Win32Window::onCreate()
{
  Window::onCreate();
  if (this->options_.visible)
  {
    this->visibility.set(true, true);
  }
}

void Win32Window::onShow()
{
  if (this->hwnd_)
    ShowWindow(this->hwnd_, SW_SHOW);
}

void Win32Window::onHide()
{
  // Win32ではdestroyNativeWindowで破棄するため、ここでSW_HIDEは不要
}

void Win32Window::mountScene()
{
  if (nodeManager_ || !this->hwnd_)
  {
    return;
  }
  declara::core::scene::Scene *currentScene = this->scene();
  if (!currentScene)
  {
    return;
  }
  scenePlatformController_ = new Win32ScenePlatformController(this->hwnd_);
  nodeManager_ = new declara::core::scene::StaticNodeManager();
  nodeManager_->mount(currentScene, scenePlatformController_);
}

void Win32Window::teardownScene()
{
  if (nodeManager_)
  {
    delete nodeManager_;
    nodeManager_ = 0;
  }
  if (scenePlatformController_)
  {
    delete scenePlatformController_;
    scenePlatformController_ = 0;
  }
}

bool Win32Window::handleCommand(WPARAM wParam, LPARAM lParam)
{
  if (!scenePlatformController_)
  {
    return false;
  }
  return scenePlatformController_->handleCommand(wParam, lParam);
}
