#include "Win32RectSurfaceRedrawTests.hpp"
#include "support/TestVerify.hpp"
#include <cstdio>
#include <windows.h>
#include "Win32ScenePlatformController.hpp"
#include "app/RectSurface.hpp"
#include "context/Win32RectSurfaceContext.hpp"
#include "core/State.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "testing/Win32ScenePlatformTestAccess.hpp"

namespace
{
  LRESULT CALLBACK countHostPaint(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
  {
    if (message == WM_PAINT || message == WM_ERASEBKGND)
    {
      Win32ScenePlatformController::noteNativePaint(
          hwnd, Win32ScenePlatformController::NATIVE_PAINT_ROOT, message == WM_ERASEBKGND);
    }
    WNDPROC original = reinterpret_cast<WNDPROC>(GetClassLongPtrW(hwnd, GCLP_WNDPROC));
    return CallWindowProcW(original, hwnd, message, wParam, lParam);
  }

  void pumpMessages()
  {
    MSG message;
    while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
  }
}

void testWin32RectSurfaceTicksRepaintOnlySurface()
{
  typedef loka::dsl::testing::Win32ScenePlatformTestAccess Access;
  HWND root = CreateWindowExW(0, L"STATIC", L"rect-surface-redraw-host", WS_OVERLAPPED,
                              0, 0, 320, 240, NULL, NULL, GetModuleHandleW(NULL), NULL);
  LOKA_VERIFY(root != NULL);
  const LONG_PTR original = SetWindowLongPtrW(root, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&countHostPaint));
  LOKA_VERIFY(original != 0);
  {
    Win32ScenePlatformController controller(root, loka::win32::Win32DisplayScale(96));
    loka::core::PushStateTracker tracker;
    loka::core::MutableState<loka::app::RectSurfaceModel> model((loka::app::RectSurfaceModel()));
    tracker.addState(&model);
    loka::app::RectSurfaceProps props;
    props.model(&model).size(100, 60);
    loka::app::RectSurfaceNode node(props);
    Win32RectSurfaceContext context(&controller, root, 10, 20, 100, 60, &node);
    LOKA_VERIFY(context.hasNativeSurface());
    ShowWindow(root, SW_SHOWNOACTIVATE);
    Access::flushPendingInvalidations(controller);
    pumpMessages();
    for (int tick = 1; tick <= 5; ++tick)
    {
      const Access::RedrawStats before = Access::redrawStats(controller);
      loka::app::RectSurfaceModel next;
      next.rectCount = 1;
      next.rects[0] = loka::app::RectSprite(static_cast<short>(tick * 4), 8, 10, 10);
      {
        loka::core::StateTrackerGuard guard(&tracker);
        model.set(next);
      }
      Access::flushPendingInvalidations(controller);
      pumpMessages();
      const Access::RedrawStats after = Access::redrawStats(controller);
      const int rootErase = after.rootEraseCount - before.rootEraseCount;
      const int rootPaint = after.rootPaintCount - before.rootPaintCount;
      const int surfaceErase = after.rectSurfaceEraseCount - before.rectSurfaceEraseCount;
      const int surfacePaint = after.rectSurfacePaintCount - before.rectSurfacePaintCount;
      std::printf("#597 tick %d: root erase=%d paint=%d; surface erase=%d paint=%d\n",
                  tick, rootErase, rootPaint, surfaceErase, surfacePaint);
      LOKA_VERIFY(rootErase == 0);
      LOKA_VERIFY(rootPaint == 0);
      LOKA_VERIFY(surfaceErase == 0);
      LOKA_VERIFY(surfacePaint == 1);
    }
    context.onFactChanged(loka::app::scene::NODE_FACT_ATTACHED, loka::app::scene::NODE_FACT_RETIRED);
    controller.drainNativeRetirements();
  }
  SetWindowLongPtrW(root, GWLP_WNDPROC, original);
  DestroyWindow(root);
}
