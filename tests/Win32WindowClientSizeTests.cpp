#include "Win32WindowClientSizeTests.hpp"
#include "support/TestVerify.hpp"
#include <cassert>
#include <cstdio>
#include <windows.h>
#include "Win32App.hpp"
#include "Win32Window.hpp"
#include "app/Menu.hpp"
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
  assert(window.frameState().get().width == declaredWidth);
  assert(window.frameState().get().height == declaredHeight);

  MenuApplyingWin32App app;
  app.setActiveWindow(&window);
  app.setDefaultMenuBar(&menuBar);
  assert(GetMenu(hwnd) && "the production menu path must attach the declared menu");
  assertClientSize(hwnd, declaredWidth, declaredHeight);
  assert(window.frameState().get().width == declaredWidth);
  assert(window.frameState().get().height == declaredHeight);

  app.setDefaultMenuBar(0);
  assert(!GetMenu(hwnd) && "the production menu path must detach a cleared menu");
  assertClientSize(hwnd, declaredWidth, declaredHeight);
  assert(window.frameState().get().width == declaredWidth);
  assert(window.frameState().get().height == declaredHeight);
  app.setActiveWindow(0);

  const int resizedWidth = 311;
  const int resizedHeight = 197;
  setWindowFrame(window, loka::core::Frame(40, 40, resizedWidth, resizedHeight));
  assertClientSize(hwnd, resizedWidth, resizedHeight);
  assert(window.frameState().get().width == resizedWidth);
  assert(window.frameState().get().height == resizedHeight);

  setWindowVisibility(window, false);
  printf("==== [testWin32DeclaredWindowSizeMeansClientArea] PASSED ====\n");
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
