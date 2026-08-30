#include "Win32WindowClientSizeTests.hpp"
#include "support/TestVerify.hpp"
#include <cassert>
#include <cstdio>
#include <windows.h>
#include "Win32App.hpp"
#include "Win32Window.hpp"
#include "app/Menu.hpp"
#include "core/State.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "platform/null/NullPlatformContext.hpp"

namespace
{
  class MenuApplyingWin32App : public Win32App
  {
  public:
    MenuApplyingWin32App()
        : Win32App(0, GetModuleHandle(NULL), SW_SHOW)
    {
    }

    virtual ~MenuApplyingWin32App() {}
  };

  void setWindowVisibility(Win32Window &window, bool visible)
  {
    loka::core::StateTrackerGuard guard(window.getTracker());
    window.visibilityState().set(visible, true);
  }

  void setWindowFrame(Win32Window &window, const loka::core::Frame &frame)
  {
    loka::core::StateTrackerGuard guard(window.getTracker());
    window.frameState().set(frame);
  }

  void assertClientSize(HWND hwnd, int expectedWidth, int expectedHeight)
  {
    RECT client;
    LOKA_VERIFY(GetClientRect(hwnd, &client));
    const int actualWidth = client.right - client.left;
    const int actualHeight = client.bottom - client.top;
    printf("  declared=%dx%d client=%dx%d\n", expectedWidth, expectedHeight, actualWidth, actualHeight);
    fflush(stdout);
    assert(actualWidth == expectedWidth &&
           "declared window width must describe the Win32 client area");
    assert(actualHeight == expectedHeight &&
           "declared window height must describe the Win32 client area");
  }

  RECT readWindowRect(HWND hwnd)
  {
    RECT rect;
    LOKA_VERIFY(GetWindowRect(hwnd, &rect));
    return rect;
  }

  void pumpWindowMessages(unsigned int settleCycles)
  {
    for (unsigned int cycle = 0; cycle < settleCycles; ++cycle)
    {
      MSG message;
      while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
      {
        TranslateMessage(&message);
        DispatchMessageW(&message);
      }
    }
  }

  bool sameRect(const RECT &lhs, const RECT &rhs)
  {
    return lhs.left == rhs.left && lhs.top == rhs.top &&
           lhs.right == rhs.right && lhs.bottom == rhs.bottom;
  }

  void CountFrameNotification(void *userData)
  {
    int *count = static_cast<int *>(userData);
    if (count)
    {
      ++*count;
    }
  }

  struct Win32WidthIsNarrow : public loka::core::DerivedState<bool>::EvalFn
  {
    const loka::core::State<loka::core::Frame> *frame;

    explicit Win32WidthIsNarrow(const loka::core::State<loka::core::Frame> *frameState)
        : frame(frameState)
    {
    }

    virtual bool operator()()
    {
      return this->frame && this->frame->get().hasSize() && this->frame->get().width < 480;
    }
  };

  void resizeNativeClient(HWND hwnd, int width, int height)
  {
    RECT outer = {0, 0, width, height};
    const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
    const DWORD exStyle = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
    LOKA_VERIFY(AdjustWindowRectEx(&outer, style, GetMenu(hwnd) ? TRUE : FALSE, exStyle));
    LOKA_VERIFY(SetWindowPos(hwnd,
                            NULL,
                            0,
                            0,
                            outer.right - outer.left,
                            outer.bottom - outer.top,
                            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE));
  }
} // namespace

void testWin32DeclaredWindowSizeMeansClientArea()
{
  printf("\n==== [testWin32DeclaredWindowSizeMeansClientArea] start ====\n");
  const int declaredWidth = 257;
  const int declaredHeight = 163;
  loka::app::MenuBarDefinition menuBar;
  menuBar << (loka::app::Menu("File") << loka::app::MenuItem("Quit"));
  WindowProps props;
  props.frame(40, 40, declaredWidth, declaredHeight).visible(false);
  NullPlatformContext context;
  Win32Window window(&context, props);

  setWindowVisibility(window, true);
  HWND hwnd = window.hwnd();
  assert(hwnd && IsWindow(hwnd));

  assertClientSize(hwnd, declaredWidth, declaredHeight);
  LOKA_VERIFY(window.frameState().get().width == declaredWidth);
  LOKA_VERIFY(window.frameState().get().height == declaredHeight);

  MenuApplyingWin32App app;
  app.setActiveWindow(&window);
  app.setDefaultMenuBar(&menuBar);
  assert(GetMenu(hwnd) && "the production menu path must attach the declared menu");
  assertClientSize(hwnd, declaredWidth, declaredHeight);
  LOKA_VERIFY(window.frameState().get().width == declaredWidth);
  LOKA_VERIFY(window.frameState().get().height == declaredHeight);

  app.setDefaultMenuBar(0);
  assert(!GetMenu(hwnd) && "the production menu path must detach a cleared menu");
  assertClientSize(hwnd, declaredWidth, declaredHeight);
  LOKA_VERIFY(window.frameState().get().width == declaredWidth);
  LOKA_VERIFY(window.frameState().get().height == declaredHeight);
  app.setActiveWindow(0);

  const int resizedWidth = 311;
  const int resizedHeight = 197;
  setWindowFrame(window, loka::core::Frame(40, 40, resizedWidth, resizedHeight));
  assertClientSize(hwnd, resizedWidth, resizedHeight);
  LOKA_VERIFY(window.frameState().get().width == resizedWidth);
  LOKA_VERIFY(window.frameState().get().height == resizedHeight);

  setWindowVisibility(window, false);
  printf("==== [testWin32DeclaredWindowSizeMeansClientArea] PASSED ====\n");
}

void testWin32ResizeMessageStoresTrackedContentSize()
{
  printf("\n==== [testWin32ResizeMessageStoresTrackedContentSize] start ====\n");
  WindowProps props;
  props.frame(40, 40, 640, 360).visible(false);
  NullPlatformContext context;
  Win32Window window(&context, props);
  setWindowVisibility(window, true);
  HWND hwnd = window.hwnd();
  assert(hwnd && IsWindow(hwnd));

  const loka::core::Frame declaredFrame = window.frameState().get();
  const loka::core::Frame beforeMinimize = window.nativeFrame().get();
  SendMessageW(hwnd, WM_SIZE, SIZE_MINIMIZED, MAKELPARAM(0, 0));
  LOKA_VERIFY(window.nativeFrame().get() == beforeMinimize &&
              "a minimized WM_SIZE must not replace the last native content frame");
  LOKA_VERIFY(window.frameState().get() == declaredFrame &&
              "a minimized WM_SIZE must not change the declared content frame");

  loka::core::State<loka::core::Frame> &nativeFrame = window.nativeFrame();
  loka::core::DerivedState<bool> narrow(&nativeFrame, new Win32WidthIsNarrow(&nativeFrame));
  loka::core::PushStateTracker *tracker = window.getTracker()->asPushTracker();
  assert(tracker);
  tracker->addState(&narrow);
  int notifications = 0;
  narrow.bind(&CountFrameNotification, &notifications, false);

  resizeNativeClient(hwnd, 400, 280);
  const loka::core::Frame resized = window.nativeFrame().get();
  assert(resized.width == 400 && resized.height == 280 &&
         "a normal WM_SIZE must publish the Win32 native content size");
  LOKA_VERIFY(window.frameState().get() == declaredFrame &&
              "a native WM_SIZE must not echo into the declared content frame");
  assert(narrow.get() &&
         "derived window state must settle before the native resize transaction returns");
  assert(notifications == 1);

  tracker->removeState(&narrow);
  setWindowVisibility(window, false);
  printf("==== [testWin32ResizeMessageStoresTrackedContentSize] PASSED ====\n");
}

void testWin32NativeWindowCreationDoesNotEchoVisibility()
{
  printf("\n==== [testWin32NativeWindowCreationDoesNotEchoVisibility] start ====\n");
  WindowProps props;
  props.frame(40, 40, 257, 163).visible(false);
  NullPlatformContext context;
  Win32Window window(&context, props);
  int notifications = 0;
  window.visibilityState().bind(&CountFrameNotification, &notifications, false);

  setWindowVisibility(window, true);
  HWND hwnd = window.hwnd();
  assert(hwnd && IsWindow(hwnd));
  LOKA_VERIFY(IsWindowVisible(hwnd));
  printf("  visibility notifications after show=%d\n", notifications);
  fflush(stdout);
  assert(notifications == 1 &&
         "native window creation must not write visibilityState back (one application write, one notification)");

  setWindowVisibility(window, false);
  const HWND destroyedHwnd = window.hwnd();
  LOKA_VERIFY(destroyedHwnd == NULL);
  assert(notifications == 2 &&
         "native window destruction must not write visibilityState back either");

  window.visibilityState().unbind(&CountFrameNotification, &notifications);
  printf("==== [testWin32NativeWindowCreationDoesNotEchoVisibility] PASSED ====\n");
}

void testWin32AppOnlyMenuWindowSettles()
{
  printf("\n==== [testWin32AppOnlyMenuWindowSettles] start ====\n");
  const int declaredWidth = 257;
  const int declaredHeight = 163;
  loka::app::MenuBarDefinition appOnlyMenuBar;
  appOnlyMenuBar << (loka::app::AppMenu() << loka::app::MenuItem("About").actionType(
                                                loka::app::MENU_ACTION_ABOUT_APP));
  WindowProps props;
  props.frame(40, 40, declaredWidth, declaredHeight).visible(false);
  NullPlatformContext context;
  Win32Window window(&context, props);

  setWindowVisibility(window, true);
  HWND hwnd = window.hwnd();
  assert(hwnd && IsWindow(hwnd));
  const RECT initialOuterRect = readWindowRect(hwnd);

  MenuApplyingWin32App app;
  app.setActiveWindow(&window);
  app.setDefaultMenuBar(&appOnlyMenuBar);
  pumpWindowMessages(16);

  const RECT settledOuterRect = readWindowRect(hwnd);
  printf("  initial outer=%ld,%ld-%ld,%ld settled outer=%ld,%ld-%ld,%ld\n",
         initialOuterRect.left,
         initialOuterRect.top,
         initialOuterRect.right,
         initialOuterRect.bottom,
         settledOuterRect.left,
         settledOuterRect.top,
         settledOuterRect.right,
         settledOuterRect.bottom);
  fflush(stdout);
  assert(sameRect(initialOuterRect, settledOuterRect) &&
         "an app-only menu must not grow the native window while it settles");
  assertClientSize(hwnd, declaredWidth, declaredHeight);

  app.setActiveWindow(0);
  setWindowVisibility(window, false);
  printf("==== [testWin32AppOnlyMenuWindowSettles] PASSED ====\n");
}

void testWin32MenuRebuildPreservesMovedWindowFrame()
{
  printf("\n==== [testWin32MenuRebuildPreservesMovedWindowFrame] start ====\n");
  const int declaredX = 40;
  const int declaredY = 40;
  const int declaredWidth = 257;
  const int declaredHeight = 163;
  const int movedX = 233;
  const int movedY = 177;
  loka::app::MenuBarDefinition initialMenuBar;
  initialMenuBar << (loka::app::Menu("File") << loka::app::MenuItem("Initial"));
  loka::app::MenuBarDefinition rebuiltMenuBar;
  rebuiltMenuBar << (loka::app::Menu("File") << loka::app::MenuItem("Rebuilt"));
  WindowProps props;
  props.frame(declaredX, declaredY, declaredWidth, declaredHeight).visible(false);
  NullPlatformContext context;
  Win32Window window(&context, props);

  setWindowVisibility(window, true);
  HWND hwnd = window.hwnd();
  assert(hwnd && IsWindow(hwnd));

  MenuApplyingWin32App app;
  app.setActiveWindow(&window);
  app.setDefaultMenuBar(&initialMenuBar);
  assert(GetMenu(hwnd) && "the production menu path must attach the initial menu");
  LOKA_VERIFY(SetWindowPos(hwnd,
                           NULL,
                           movedX,
                           movedY,
                           0,
                           0,
                           SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));
  const RECT movedOuterRect = readWindowRect(hwnd);
  RECT movedClientRect;
  LOKA_VERIFY(GetClientRect(hwnd, &movedClientRect));

  app.setDefaultMenuBar(&rebuiltMenuBar);

  const RECT rebuiltOuterRect = readWindowRect(hwnd);
  RECT rebuiltClientRect;
  LOKA_VERIFY(GetClientRect(hwnd, &rebuiltClientRect));
  printf("  declared origin=%d,%d moved origin=%ld,%ld rebuilt origin=%ld,%ld client=%ldx%ld -> %ldx%ld\n",
         declaredX,
         declaredY,
         movedOuterRect.left,
         movedOuterRect.top,
         rebuiltOuterRect.left,
         rebuiltOuterRect.top,
         movedClientRect.right - movedClientRect.left,
         movedClientRect.bottom - movedClientRect.top,
         rebuiltClientRect.right - rebuiltClientRect.left,
         rebuiltClientRect.bottom - rebuiltClientRect.top);
  fflush(stdout);
  assert(rebuiltOuterRect.left == movedOuterRect.left &&
         rebuiltOuterRect.top == movedOuterRect.top &&
         "menu rebuild must preserve the actual Win32 window position");
  assert(sameRect(movedClientRect, rebuiltClientRect) &&
         "menu rebuild must preserve the Win32 client size");

  app.setActiveWindow(0);
  setWindowVisibility(window, false);
  printf("==== [testWin32MenuRebuildPreservesMovedWindowFrame] PASSED ====\n");
}

void testWin32WindowDestructionDestroysNativeWindow()
{
  printf("\n==== [testWin32WindowDestructionDestroysNativeWindow] start ====\n");
  HWND hwnd = NULL;
  {
    WindowProps props;
    props.frame(40, 40, 257, 163).visible(false);
    NullPlatformContext context;
    Win32Window window(&context, props);
    setWindowVisibility(window, true);
    hwnd = window.hwnd();
    LOKA_VERIFY(hwnd != NULL);
    const BOOL windowCreated = IsWindow(hwnd);
    LOKA_VERIFY(windowCreated);
  }
  const BOOL windowRemaining = IsWindow(hwnd);
  LOKA_VERIFY(!windowRemaining);
  printf("==== [testWin32WindowDestructionDestroysNativeWindow] PASSED ====\n");
}

void testWin32RepeatedAppDestructionDetachesMenuBeforeDestroyingHandle()
{
  printf("\n==== [testWin32RepeatedAppDestructionDetachesMenuBeforeDestroyingHandle] start ====\n");
  loka::app::MenuBarDefinition menuBar;
  menuBar << (loka::app::Menu("File") << loka::app::MenuItem("Quit"));
  WindowProps props;
  props.frame(40, 40, 257, 163).visible(false);
  NullPlatformContext context;
  Win32Window window(&context, props);
  setWindowVisibility(window, true);
  HWND hwnd = window.hwnd();
  LOKA_VERIFY(hwnd != NULL);
  int frameNotifications = 0;
  window.frameState().deferBind(&CountFrameNotification, &frameNotifications);

  for (int run = 0; run < 2; ++run)
  {
    HMENU menu = NULL;
    {
      MenuApplyingWin32App app;
      app.setActiveWindow(&window);
      app.setDefaultMenuBar(&menuBar);
      menu = GetMenu(hwnd);
      LOKA_VERIFY(menu != NULL);
      const BOOL menuCreated = IsMenu(menu);
      LOKA_VERIFY(menuCreated);
      frameNotifications = 0;
    }

    // App teardown may alter native menu ownership but must publish no State.
    LOKA_VERIFY(frameNotifications == 0);
    assertClientSize(hwnd, 257, 163);
    const HMENU attachedMenu = GetMenu(hwnd);
    LOKA_VERIFY(attachedMenu == NULL);
    const BOOL menuRemaining = IsMenu(menu);
    LOKA_VERIFY(!menuRemaining);
  }

  const BOOL windowRemaining = IsWindow(hwnd);
  LOKA_VERIFY(windowRemaining);
  window.frameState().deferUnbind(&CountFrameNotification, &frameNotifications);
  setWindowVisibility(window, false);
  printf("==== [testWin32RepeatedAppDestructionDetachesMenuBeforeDestroyingHandle] PASSED ====\n");
}

void testWin32NativeWindowDestructionReleasesMenuWithoutStateNotification()
{
  printf("\n==== [testWin32NativeWindowDestructionReleasesMenuWithoutStateNotification] start ====\n");
  loka::app::MenuBarDefinition menuBar;
  menuBar << (loka::app::Menu("File") << loka::app::MenuItem("Quit"));
  MenuApplyingWin32App app;
  WindowProps props;
  props.frame(40, 40, 257, 163).visible(false);
  NullPlatformContext context;
  Win32Window window(&context, props);
  window.setApp(&app);
  setWindowVisibility(window, true);
  app.setDefaultMenuBar(&menuBar);
  HMENU menu = GetMenu(window.hwnd());
  LOKA_VERIFY(menu != NULL);
  const BOOL menuCreated = IsMenu(menu);
  LOKA_VERIFY(menuCreated);
  int frameNotifications = 0;
  window.frameState().deferBind(&CountFrameNotification, &frameNotifications);

  setWindowVisibility(window, false);

  LOKA_VERIFY(frameNotifications == 0);
  const HWND destroyedHwnd = window.hwnd();
  LOKA_VERIFY(destroyedHwnd == NULL);
  Window *activeWindow = app.activeWindow();
  LOKA_VERIFY(activeWindow == NULL);
  const BOOL menuRemaining = IsMenu(menu);
  LOKA_VERIFY(!menuRemaining);
  window.frameState().deferUnbind(&CountFrameNotification, &frameNotifications);
  printf("==== [testWin32NativeWindowDestructionReleasesMenuWithoutStateNotification] PASSED ====\n");
}
