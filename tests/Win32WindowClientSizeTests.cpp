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
