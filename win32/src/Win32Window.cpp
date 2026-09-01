#include "Win32Window.hpp"
#include "Win32App.hpp"
#include <windows.h>
#include <string>
#include "app/core/Window.hpp"
#include "core/StateTracker.hpp"

#include "app/scene/Scene.hpp"
#include "Win32ScenePlatformController.hpp"
#include "context/Win32OpenFileDialogContext.hpp"
#include "core/String.hpp"
#include "platform/Win32String.hpp"
#include "platform/Win32DisplayScale.hpp"

namespace
{
  static bool g_classRegistered = false;
  static const wchar_t *kWndClassName = L"DevWndClass";
  static const DWORD kWindowStyle = WS_OVERLAPPEDWINDOW;
  static const DWORD kWindowExStyle = WS_EX_CONTROLPARENT;
  // Windows SDK headers hide this message when the XP compatibility preset
  // sets _WIN32_WINNT=0x0501, although newer Windows can still send it to the
  // same binary. Keep the wire value local to the capability-aware rail.
  static const UINT kWindowDpiChangedMessage = 0x02E0;

  bool CalculateOuterSizeForClient(int clientWidth,
                                   int clientHeight,
                                   DWORD style,
                                   DWORD exStyle,
                                   BOOL hasMenu,
                                   const loka::win32::Win32DisplayScale &scale,
                                   int &outerWidth,
                                   int &outerHeight)
  {
    RECT rect = {0,
                 0,
                 scale.projectLength(clientWidth),
                 scale.projectLength(clientHeight)};
    if (!scale.adjustWindowRect(rect, style, hasMenu, exStyle))
    {
      return false;
    }
    outerWidth = rect.right - rect.left;
    outerHeight = rect.bottom - rect.top;
    return true;
  }
} // namespace

Win32Window::Win32Window(PlatformContext *context, const WindowProps &props)
    : Window(context, props),
      hwnd_(NULL),
      app_(NULL),
      scenePlatformController_(0)
{
  // Track logical states and project them into native Win32 state.
  this->observeNativeState(this->visibilityState(), &Win32Window::VisibilityChangedThunk, this);
  this->observeNativeState(this->displayTitleState(), &Win32Window::TitleChangedThunk, this);
  this->observeNativeState(this->frameState(), &Win32Window::FrameChangedThunk, this);
}

Win32Window::~Win32Window()
{
  this->detachNativeStateObservers();
  if (this->hwnd_)
  {
    this->destroyNativeWindow();
  }
  else
  {
    this->teardownScene();
  }
}

bool Win32Window::queryNativeContentFrame(loka::core::Frame &out) const
{
  if (!this->hwnd_)
  {
    return false;
  }
  RECT windowRect;
  RECT clientRect;
  if (!GetWindowRect(this->hwnd_, &windowRect) || !GetClientRect(this->hwnd_, &clientRect))
  {
    return false;
  }
  out = loka::win32::Win32DisplayScale::forWindow(this->hwnd_)
            .windowContentFrameFromNative(windowRect,
                                          clientRect.right - clientRect.left,
                                          clientRect.bottom - clientRect.top);
  return true;
}

bool Win32Window::storeCurrentNativeContentFrame()
{
  loka::core::Frame frame;
  if (!this->queryNativeContentFrame(frame))
  {
    return false;
  }
  this->storeNativeFrame(frame);
  return true;
}

bool Win32Window::applyNativeContentFrame(const loka::core::Frame &frame)
{
  if (!this->hwnd_ || !frame.hasSize())
  {
    return false;
  }
  RECT windowRect;
  if (!GetWindowRect(this->hwnd_, &windowRect))
  {
    return false;
  }
  int outerWidth = 0;
  int outerHeight = 0;
  const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(this->hwnd_, GWL_STYLE));
  const DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(this->hwnd_, GWL_EXSTYLE));
  const loka::win32::Win32DisplayScale scale =
      loka::win32::Win32DisplayScale::forWindow(this->hwnd_);
  if (!CalculateOuterSizeForClient(frame.width,
                                   frame.height,
                                   style,
                                   exStyle,
                                   GetMenu(this->hwnd_) ? TRUE : FALSE,
                                   scale,
                                   outerWidth,
                                   outerHeight))
  {
    assert(false && "Win32 client size must convert to an outer window size");
    return false;
  }
  const int x = frame.x >= 0 ? frame.x : windowRect.left;
  const int y = frame.y >= 0 ? frame.y : windowRect.top;
  if (x == windowRect.left && y == windowRect.top &&
      outerWidth == windowRect.right - windowRect.left &&
      outerHeight == windowRect.bottom - windowRect.top)
  {
    return true;
  }
  return MoveWindow(this->hwnd_, x, y, outerWidth, outerHeight, TRUE) != FALSE;
}

bool Win32Window::detachMenuForTeardown(HMENU expectedMenu)
{
  if (!this->hwnd_ || !expectedMenu || GetMenu(this->hwnd_) != expectedMenu)
  {
    return false;
  }
  loka::core::Frame contentFrame;
  if (!this->queryNativeContentFrame(contentFrame))
  {
    return false;
  }
  const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(this->hwnd_, GWL_STYLE));
  const DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(this->hwnd_, GWL_EXSTYLE));
  const loka::win32::Win32DisplayScale scale =
      loka::win32::Win32DisplayScale::forWindow(this->hwnd_);
  int outerWidth = 0;
  int outerHeight = 0;
  if (!CalculateOuterSizeForClient(
          contentFrame.width,
          contentFrame.height,
          style,
          exStyle,
          FALSE,
          scale,
          outerWidth,
          outerHeight))
  {
    return false;
  }

  const LONG_PTR userData = GetWindowLongPtrW(this->hwnd_, GWLP_USERDATA);
  SetWindowLongPtrW(this->hwnd_, GWLP_USERDATA, 0);
  const BOOL detached = SetMenu(this->hwnd_, NULL);
  if (detached)
  {
    MoveWindow(this->hwnd_,
               contentFrame.x,
               contentFrame.y,
               outerWidth,
               outerHeight,
               TRUE);
  }
  SetWindowLongPtrW(this->hwnd_, GWLP_USERDATA, userData);
  return detached != FALSE;
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
      self->onHide();
      self->destroyNativeWindow();
    }
  }
}

void Win32Window::TitleChangedThunk(void *userData)
{
  Win32Window *self = static_cast<Win32Window *>(userData);
  if (self && self->hwnd_)
  {
    std::wstring wide;
    if (loka::win32::MaterializeWideString(self->displayTitleState().get(), wide))
    {
      SetWindowTextW(self->hwnd_, wide.c_str());
    }
    else
    {
      SetWindowTextW(self->hwnd_, L"");
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
  self->applyNativeContentFrame(frame);
}

LRESULT CALLBACK Win32Window::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  Win32Window *self = NULL;
  if (msg == WM_NCCREATE)
  {
    CREATESTRUCTW *cs = reinterpret_cast<CREATESTRUCTW *>(lParam);
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
    if (Win32OpenFileDialogContext::handlePostedResultMessage(msg, wParam, lParam))
    {
      return 0;
    }
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
      return DefWindowProcW(hwnd, msg, wParam, lParam);
    case WM_SIZE:
      if (self->scenePlatformController_)
      {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        self->scenePlatformController_->relayoutNativeClientPixels(width, height);
      }
      if (wParam != SIZE_MINIMIZED && self->hwnd_)
      {
        self->storeCurrentNativeContentFrame();
      }
      return 0;
    case kWindowDpiChangedMessage:
    {
      const loka::win32::Win32DisplayScale nextScale(LOWORD(wParam));
      if (self->scenePlatformController_)
      {
        self->scenePlatformController_->updateDisplayScale(nextScale);
      }
      const RECT *suggested = reinterpret_cast<const RECT *>(lParam);
      if (suggested)
      {
        SetWindowPos(hwnd,
                     NULL,
                     suggested->left,
                     suggested->top,
                     suggested->right - suggested->left,
                     suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
      }
      if (self->scenePlatformController_)
      {
        // SetWindowPos commonly reaches WM_SIZE and lays out once already, but
        // a size-preserving suggested rectangle need not. The projection pass
        // is idempotent, so finish from the current client pixels either way.
        RECT client;
        if (GetClientRect(hwnd, &client))
        {
          self->scenePlatformController_->relayoutNativeClientPixels(
              client.right - client.left, client.bottom - client.top);
        }
      }
      self->storeCurrentNativeContentFrame();
      return 0;
    }
    case WM_MOVE:
      if (self->hwnd_ && !IsIconic(self->hwnd_))
      {
        self->storeCurrentNativeContentFrame();
      }
      return 0;
    case WM_PAINT:
    {
      Win32ScenePlatformController::noteNativePaint(hwnd, Win32ScenePlatformController::NATIVE_PAINT_ROOT, false);
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      // Clear the background with the native window color.
      RECT rc;
      GetClientRect(hwnd, &rc);
      FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW + 1));
      EndPaint(hwnd, &ps);
      break;
    }
    case WM_DESTROY:
      self->onDestroy();
      self->hwnd_ = NULL;
      if (self->app_)
      {
        self->app_->requestWindowClose(static_cast<Window *>(self));
      }
      break;
    default:
      return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void Win32Window::createNativeWindow()
{
  if (!g_classRegistered)
  {
    // Unicode window class: the title and other text messages stay UTF-16;
    // WndProc must pair with DefWindowProcW below.
    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = Win32Window::WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = kWndClassName;
    RegisterClassW(&wc);
    g_classRegistered = true;
  }
  const loka::core::Frame defaultFrame = Window::defaultFrame();
  const int clientWidth = this->hasSize() ? this->width() : defaultFrame.width;
  const int clientHeight = this->hasSize() ? this->height() : defaultFrame.height;
  const loka::win32::Win32DisplayScale initialScale =
      loka::win32::Win32DisplayScale::forSystem();
  int outerWidth = 0;
  int outerHeight = 0;
  if (!CalculateOuterSizeForClient(
          clientWidth,
          clientHeight,
          kWindowStyle,
          kWindowExStyle,
          FALSE,
          initialScale,
          outerWidth,
          outerHeight))
  {
    assert(false && "Win32 client size must convert to an outer window size");
    return;
  }
  HWND hwnd = CreateWindowExW(kWindowExStyle,
                              kWndClassName,
                              L"",
                              kWindowStyle,
                              this->hasPosition() ? this->positionX() : defaultFrame.x,
                              this->hasPosition() ? this->positionY() : defaultFrame.y,
                              outerWidth,
                              outerHeight,
                              NULL,
                              NULL,
                              GetModuleHandle(NULL),
                              this);
  if (hwnd)
  {
    this->hwnd_ = hwnd;
    this->applyNativeContentFrame(
        loka::core::Frame(this->hasPosition() ? this->positionX() : defaultFrame.x,
                          this->hasPosition() ? this->positionY() : defaultFrame.y,
                          clientWidth,
                          clientHeight));
    this->storeCurrentNativeContentFrame();
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
    App *appToClear = this->app_ && this->app_->activeWindow() == this ? this->app_ : 0;
    teardownScene();
    SetWindowLongPtr(this->hwnd_, GWLP_USERDATA, 0);
    DestroyWindow(this->hwnd_);
    this->hwnd_ = NULL;
    if (appToClear)
    {
      appToClear->setActiveWindow(0);
    }
  }
}

void Win32Window::onCreate()
{
  // Native creation is entered from VisibilityChangedThunk; it reads the
  // application's intent and never writes it back (no forced re-notify).
  Window::onCreate();
}

void Win32Window::onShow()
{
  if (this->hwnd_)
    ShowWindow(this->hwnd_, SW_SHOW);
}

void Win32Window::onHide()
{
  // Native hide is handled by destroyNativeWindow().
}

void Win32Window::synchronizeScenePlatform()
{
  if (scenePlatformController_)
  {
    scenePlatformController_->synchronize();
  }
}

void Win32Window::drainNativeRetirements()
{
  if (this->scenePlatformController_)
  {
    this->scenePlatformController_->drainNativeRetirements();
  }
}

bool Win32Window::hasPendingScenePlatformSync() const
{
  return scenePlatformController_ ? scenePlatformController_->hasPendingSync() : false;
}

bool Win32Window::queryDisplayScalePercent(int &out) const
{
  if (!hwnd_)
  {
    return false;
  }
  loka::win32::Win32DisplayScale scale;
  if (!loka::win32::Win32DisplayScale::queryForWindow(this->hwnd_, scale))
  {
    return false;
  }
  out = scale.percent();
  return true;
}

bool Win32Window::queryDisplayDepth(int &out) const
{
  if (!hwnd_)
  {
    return false;
  }
  HDC dc = GetDC(hwnd_);
  if (!dc)
  {
    return false;
  }
  const int bitsPerPixel = GetDeviceCaps(dc, BITSPIXEL) * GetDeviceCaps(dc, PLANES);
  ReleaseDC(hwnd_, dc);
  if (bitsPerPixel <= 0)
  {
    return false;
  }
  out = bitsPerPixel;
  return true;
}

bool Win32Window::queryDisplayAppearance(DisplayAppearance &out) const
{
  // AppsUseLightTheme arrived with Windows 10. On an older system the value is
  // absent, and absent is the honest answer: those systems have no light/dark
  // distinction to report, so declining is not a failure path.
  HKEY key = 0;
  if (RegOpenKeyExW(HKEY_CURRENT_USER,
                    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                    0,
                    KEY_QUERY_VALUE,
                    &key)
      != ERROR_SUCCESS)
  {
    return false;
  }
  DWORD value = 0;
  DWORD size = sizeof(value);
  DWORD type = 0;
  const LONG status = RegQueryValueExW(key, L"AppsUseLightTheme", 0, &type, reinterpret_cast<LPBYTE>(&value), &size);
  RegCloseKey(key);
  if (status != ERROR_SUCCESS || type != REG_DWORD)
  {
    return false;
  }
  out = value == 0 ? DISPLAY_APPEARANCE_DARK : DISPLAY_APPEARANCE_LIGHT;
  return true;
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
  scenePlatformController_ = new Win32ScenePlatformController(
      this->hwnd_, loka::win32::Win32DisplayScale::forWindow(this->hwnd_));
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
