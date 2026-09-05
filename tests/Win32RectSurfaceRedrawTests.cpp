#include "Win32RectSurfaceRedrawTests.hpp"
#include "support/TestVerify.hpp"
#include <cstdio>
#include <windows.h>
#include "Win32ScenePlatformController.hpp"
#include "Win32Window.hpp"
#include "app/nodes/Text.hpp"
#include "context/Win32TextContext.hpp"
#include "platform/null/NullPlatformContext.hpp"
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

void testWin32ZStackTextShowsSiblingBeneath()
{
  typedef loka::dsl::testing::Win32ScenePlatformTestAccess Access;
  // Use the production root so its STATIC background and ground-fill policies
  // participate in the pin, along with the two native projection contexts.
  NullPlatformContext platform;
  WindowProps windowProps;
  windowProps.frame(40, 40, 320, 240).visible(false);
  Win32Window window(&platform, windowProps);
  {
    loka::core::StateTrackerGuard guard(window.getTracker());
    window.visibilityState().set(true);
  }
  HWND root = window.hwnd();
  LOKA_VERIFY(root != NULL);
  ShowWindow(root, SW_HIDE);
  {
    Win32ScenePlatformController controller(root, loka::win32::Win32DisplayScale(96));
    loka::app::RectSurfaceModel initial;
    initial.rectCount = 1;
    initial.rects[0] = loka::app::RectSprite(0, 40, 160, 40);
    loka::core::PushStateTracker tracker;
    loka::core::MutableState<loka::app::RectSurfaceModel> model(initial);
    tracker.addState(&model);
    loka::app::RectSurfaceProps props;
    props.model(&model).size(160, 80).clearBackground(true);
    loka::app::RectSurfaceNode node(props);
    Win32RectSurfaceContext surface(&controller, root, 0, 0, 160, 80, &node);
    LOKA_VERIFY(surface.hasNativeSurface());
    loka::app::TextNode textNode((loka::app::TextProps("*")));
    Win32TextContext text(&controller, root, 0, 0, 160, 24, &textNode);
    HWND textHwnd = FindWindowExW(root, NULL, L"STATIC", L"*");
    LOKA_VERIFY(textHwnd != NULL);

    ShowWindow(root, SW_SHOWNOACTIVATE);
    Access::flushPendingInvalidations(controller);
    pumpMessages();
    UpdateWindow(root);

    for (int phase = 0; phase < 2; ++phase)
    {
      if (phase == 1)
      {
        loka::app::RectSurfaceModel next;
        next.rectCount = 1;
        next.rects[0] = loka::app::RectSprite(0, 44, 160, 36);
        {
          loka::core::StateTrackerGuard guard(&tracker);
          model.set(next);
        }
        Access::flushPendingInvalidations(controller);
        pumpMessages();
        UpdateWindow(root);
      }
      loka::core::resource::Image capture;
      LOKA_VERIFY(Access::captureWindowClientBitmap(root, capture));
      HDC pixels = CreateCompatibleDC(NULL);
      LOKA_VERIFY(pixels != NULL);
      HGDIOBJ previous = SelectObject(pixels, static_cast<HBITMAP>(capture.nativeHandle()));
      LOKA_VERIFY(previous != NULL && previous != HGDI_ERROR);
      const COLORREF below = GetPixel(pixels, 80, 60);
      const COLORREF overlap = GetPixel(pixels, 150, 12);
      const COLORREF ground = GetPixel(pixels, 200, 100);
      int glyphPixels = 0;
      for (int y = 0; y < 24; ++y)
      {
        for (int x = 0; x < 160; ++x)
        {
          if (GetPixel(pixels, x, y) == RGB(0, 0, 0))
          {
            ++glyphPixels;
          }
        }
      }
      SelectObject(pixels, previous);
      DeleteDC(pixels);
      std::printf("#598 ZStack capture %d: below=%08lX overlap=%08lX ground=%08lX; window=%08lX glyph=%d\n",
                  phase, static_cast<unsigned long>(below), static_cast<unsigned long>(overlap),
                  static_cast<unsigned long>(ground), static_cast<unsigned long>(GetSysColor(COLOR_WINDOW)), glyphPixels);
      std::fflush(stdout);
      LOKA_VERIFY(below == RGB(0, 0, 0));
      LOKA_VERIFY(overlap == RGB(255, 255, 255));
      LOKA_VERIFY(glyphPixels > 0);
      LOKA_VERIFY(ground == GetSysColor(COLOR_WINDOW) && ground != RGB(0, 0, 0));
    }

    text.onFactChanged(loka::app::scene::NODE_FACT_ATTACHED, loka::app::scene::NODE_FACT_RETIRED);
    surface.onFactChanged(loka::app::scene::NODE_FACT_ATTACHED, loka::app::scene::NODE_FACT_RETIRED);
    controller.drainNativeRetirements();
  }
}
