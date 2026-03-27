#include "Win32Window.hpp"
#include "Win32App.hpp"
#include <windows.h>
#include <string>
#include "app/Window.hpp"
#include "loka/core/StateTracker.hpp"

#include "app/scene/Scene.hpp"
#include "app/scene/Scene.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"
#include "Win32ScenePlatformController.hpp"
#include "loka/core/String.hpp"
#include "loka/platform/StringUTF8.hpp"
#include "platform/Win32String.hpp"

namespace
{
  static bool g_classRegistered = false;
  static const char *kWndClassName = "DevWndClass";
}

Win32Window::Win32Window(PlatformContext *context, const WindowProps &props)
    : Window(context, props), hwnd_(NULL), app_(NULL), scenePlatformController_(0)
{
  // visibilityステートの変更を監視
  this->visibilityState().deferBind(&Win32Window::VisibilityChangedThunk, this);
  this->titleState().deferBind(&Win32Window::TitleChangedThunk, this);
  this->frameState().deferBind(&Win32Window::FrameChangedThunk, this);
}

Win32Window::~Win32Window()
{
  teardownScene();
}

void Win32Window::setApp(App *app)
{
  app_ = app;
  if (app_ && hwnd_)
  {
    app_->setActiveWindow(this);
  }
}

// static thunk for loka::core::State<bool>::OnChangeFn
void Win32Window::VisibilityChangedThunk(void *userData)
{
  Win32Window *self = static_cast<Win32Window *>(userData);
  if (!self)
    return;
  bool visible = self->visibilityState().get();
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
    loka::core::String titleValue = self->titleState().get();
    if (titleValue.empty())
    {
      SetWindowTextW(self->hwnd_, L"");
      return;
    }
    loka::core::Managed<loka::platform::String> handle = loka::win32::CreateWin32String(titleValue);
    loka::win32::Win32String *win32String = handle.isValid() ? dynamic_cast<loka::win32::Win32String *>(handle.get()) : 0;
    if (win32String)
      SetWindowTextW(self->hwnd_, win32String->c_str());
    else
    {
      std::string utf8;
      if (loka::platform::CollectUtf8(titleValue, utf8))
      {
        SetWindowTextA(self->hwnd_, utf8.c_str());
      }
      else
      {
        SetWindowTextW(self->hwnd_, L"");
      }
    }
  }
}

void Win32Window::FrameChangedThunk(void *userData)
{
  Win32Window *self = static_cast<Win32Window *>(userData);
  if (!self || !self->hwnd_)
  {
    return;
  }
  loka::core::Frame frame = self->frameState().get();
  if (!frame.hasSize())
  {
    return;
  }
  RECT rc;
  GetWindowRect(self->hwnd_, &rc);
  int x = frame.x >= 0 ? frame.x : rc.left;
  int y = frame.y >= 0 ? frame.y : rc.top;
  int width = frame.width > 0 ? frame.width : (rc.right - rc.left);
  int height = frame.height > 0 ? frame.height : (rc.bottom - rc.top);
  MoveWindow(self->hwnd_, x, y, width, height, TRUE);
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
    case WM_KEYDOWN:
      if (self->app_ && wParam == VK_SPACE)
      {
        if (self->app_->handleKeyPress(' '))
        {
          return 0;
        }
      }
      break;
    case WM_CHAR:
      if (self->app_)
      {
        if (wParam >= 0x20 && wParam <= 0x7E && wParam != ' ')
        {
          if (self->app_->handleKeyPress(static_cast<char>(wParam)))
          {
            return 0;
          }
        }
      }
      break;
    case WM_ACTIVATE:
      if (self->app_ && LOWORD(wParam) != WA_INACTIVE)
      {
        self->app_->setActiveWindow(static_cast<Window *>(self));
      }
      break;
    case WM_CTLCOLORSTATIC:
    {
      HDC hdc = reinterpret_cast<HDC>(wParam);
      if (hdc)
      {
        SetBkMode(hdc, TRANSPARENT);
      }
      return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
    }
    case WM_ERASEBKGND:
      Win32ScenePlatformController::noteNativePaint(hwnd, Win32ScenePlatformController::NATIVE_PAINT_ROOT, true);
      return DefWindowProc(hwnd, msg, wParam, lParam);
    case WM_SIZE:
      if (self->scenePlatformController_)
      {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        self->scenePlatformController_->relayout(width, height);
      }
      if (self->hwnd_)
      {
        RECT rc;
        if (GetWindowRect(self->hwnd_, &rc))
        {
          loka::core::Frame frame(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
          self->frameState().set(frame);
        }
      }
      return 0;
    case WM_PAINT:
    {
      Win32ScenePlatformController::noteNativePaint(hwnd, Win32ScenePlatformController::NATIVE_PAINT_ROOT, false);
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      // 背景を白で塗りつぶす
      RECT rc;
      GetClientRect(hwnd, &rc);
      FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));
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
  HWND hwnd = CreateWindowExA(
      WS_EX_CONTROLPARENT,
      kWndClassName,
      "",
      WS_OVERLAPPEDWINDOW,
      this->hasPosition() ? this->positionX() : 50,
      this->hasPosition() ? this->positionY() : 50,
      this->hasSize() ? this->width() : 300,
      this->hasSize() ? this->height() : 300,
      NULL, NULL, GetModuleHandle(NULL),
      this);
  if (hwnd)
  {
    this->hwnd_ = hwnd;
    RECT rc;
    if (GetWindowRect(hwnd, &rc))
    {
      loka::core::Frame frame(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
      this->frameState().set(frame);
    }
    if (this->app_)
    {
      this->app_->setActiveWindow(this);
    }
    TitleChangedThunk(this);
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
  if (this->visibilityState().get())
  {
    this->visibilityState().set(true, true);
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

void Win32Window::synchronizeScenePlatform()
{
  if (scenePlatformController_)
  {
    scenePlatformController_->synchronize();
  }
}

bool Win32Window::hasPendingScenePlatformSync() const
{
  return scenePlatformController_ ? scenePlatformController_->hasPendingSync() : false;
}

void Win32Window::mountScene()
{
  if (scenePlatformController_ || !this->hwnd_)
  {
    return;
  }
  loka::app::scene::Scene *currentScene = this->scene();
  if (!currentScene)
  {
    return;
  }
  scenePlatformController_ = new Win32ScenePlatformController(this->hwnd_);
  currentScene->mount(scenePlatformController_);
}

void Win32Window::teardownScene()
{
  loka::app::scene::Scene *currentScene = this->scene();
  if (currentScene)
  {
    currentScene->unmount();
  }
  if (scenePlatformController_)
  {
    delete scenePlatformController_;
    scenePlatformController_ = 0;
  }
}

bool Win32Window::handleCommand(WPARAM wParam, LPARAM lParam)
{
  if (app_ && lParam == 0)
  {
    int commandId = LOWORD(wParam);
    if (app_->handleMenuCommand(commandId, this))
    {
      return true;
    }
  }
  if (!scenePlatformController_)
  {
    return false;
  }
  return scenePlatformController_->handleCommand(wParam, lParam);
}
