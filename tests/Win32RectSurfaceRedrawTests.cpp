#include "app/nodes/controls/EditText.hpp"
#include "context/Win32EditTextContext.hpp"
#include "app/nodes/controls/PopupMenu.hpp"
#include "context/Win32PopupMenuContext.hpp"
#include "support/PropsReconciliation.hpp"
#include "app/nodes/nestable/ScrollView.hpp"
#include "app/scene/state/WriteSeat.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "Win32BuiltInSupport.hpp"
#include "support/WindowAdmissionTestApp.hpp"
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
    Win32ScenePlatformController controller(root, loka::win32::Win32DisplayScale(96, loka::app::RailMetrics()));
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

static void
exercisePaintOnlyChangeUnderScrollView(bool refused, bool clearSurface = true, bool paintBeforeApply = false)
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace PropsReconciliationSupport;
  typedef loka::dsl::testing::Win32ScenePlatformTestAccess Access;
  typedef loka::dsl::testing::SceneTestAccess SceneAccess;

  NullPlatformContext platform;
  WindowProps windowProps;
  windowProps.frame(40, 40, 320, 240).visible(false);
  Win32Window window(&platform, windowProps);
  {
    loka::core::StateTrackerGuard guard(window.getTracker());
    window.visibilityState().set(true);
  }
  WindowAdmissionTestApp admission(window);
  admission.flush();
  HWND rootHwnd = window.hwnd();
  LOKA_VERIFY(rootHwnd != NULL);
  Win32ScenePlatformController controller(rootHwnd, loka::win32::Win32DisplayScale(96, loka::app::RailMetrics()));
  RegisterWin32BuiltInSupport(controller);

  // RectSurface's palette is white ground and black sprites. Start with two
  // distinct solid surfaces, then change only A's model from white to black.
  RectSurfaceModel black;
  black.rectCount = 1;
  black.rects[0] = RectSprite(0, 0, 100, 60);
  loka::core::MutableState<RectSurfaceModel> a(clearSurface ? RectSurfaceModel() : black);
  loka::core::MutableState<RectSurfaceModel> b(black);
  loka::core::MutableState<loka::core::String> editText(loka::core::String::Literal("input"));
  loka::core::MutableState<int> selection(0);
  loka::core::MutableState<bool> enabled(true);
  const char *items[] = {"one", "two"};
  // Both native controls must be handled in the same Boundary as the surfaces.
  // The fallback case uses a genuinely unsupported non-clearing B surface.
  VStack content = VStack() << RectSurface(&a).size(100, 60).clearBackground(clearSurface) << Box().size(100, 20)
                            << RectSurface(&b).size(100, 60).clearBackground(!refused)
                            << PopupMenu(items, 2).selectedIndex(loka::app::scene::WriteSeat<int>(&selection)).enabled(&enabled) << EditText(loka::app::scene::WriteSeat<loka::core::String>(&editText));
  ScrollView declaration = ScrollView() << content;
  Scene scene((Boundary<Tree<ScrollView> >(Props<ScrollView>(&declaration))));
  scene.mount(&controller);
  SceneAccess::updateAttached(scene, true);
  settle(scene);
  BoundaryNode *boundary = SceneAccess::rootBoundary(scene);
  LOKA_VERIFY(boundary != 0);
  controller.onChange(boundary, NODE_DIRTY_NONE, false);
  controller.relayout(320, 240);
  settle(scene);
  Access::flushPendingInvalidations(controller);
  pumpMessages();
  UpdateWindow(rootHwnd);

  HWND viewport = FindWindowExW(rootHwnd, NULL, L"LOKA_SCROLL_VIEW", NULL);
  LOKA_VERIFY(viewport != NULL);
  LOKA_VERIFY((GetWindowLongPtrW(viewport, GWL_STYLE) & WS_CLIPCHILDREN) != 0);
  LOKA_VERIFY(FindWindowExW(viewport, NULL, L"COMBOBOX", NULL) != NULL);
  LOKA_VERIFY(FindWindowExW(viewport, NULL, L"EDIT", NULL) != NULL);
  HWND first = FindWindowExW(viewport, NULL, L"LOKA_RECT_SURFACE", NULL);
  LOKA_VERIFY(first != NULL);
  HWND second = FindWindowExW(viewport, first, L"LOKA_RECT_SURFACE", NULL);
  LOKA_VERIFY(second != NULL);
  RECT firstRect, secondRect;
  LOKA_VERIFY(GetWindowRect(first, &firstRect));
  LOKA_VERIFY(GetWindowRect(second, &secondRect));
  // Native z-order need not match declaration order; A is the upper surface.
  const RECT aRect = firstRect.top < secondRect.top ? firstRect : secondRect;
  const RECT bRect = firstRect.top < secondRect.top ? secondRect : firstRect;
  LOKA_VERIFY(aRect.bottom < bRect.top);
  POINT aPoint = {(aRect.left + aRect.right) / 2, (aRect.top + aRect.bottom) / 2};
  POINT bPoint = {(bRect.left + bRect.right) / 2, (bRect.top + bRect.bottom) / 2};
  LOKA_VERIFY(ScreenToClient(rootHwnd, &aPoint));
  LOKA_VERIFY(ScreenToClient(rootHwnd, &bPoint));

  for (int phase = 0; phase < 2; ++phase)
  {
    if (phase == 1)
    {
      Access::resetRedrawStats(controller);
      // Same RectSurfaceModel write as the no-LAYOUT PaintBaseline pin.
      {
        loka::core::StateTrackerGuard guard(boundary->tracker());
        a.set(clearSurface ? black : RectSurfaceModel());
      }
      // Submit the observer's HWND request without painting. The later queue
      // row must therefore come from the Boundary's answer translation.
      Access::flushPendingInvalidations(controller);
      if (paintBeforeApply)
      {
        // Native expose may run before a deferred Boundary apply. A
        // non-clearing draw still cannot certify that old sprites disappeared.
        HWND surface = firstRect.top < secondRect.top ? first : second;
        RedrawWindow(surface, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
      }
      settle(scene);
      const PlatformApplyPlan &plan = SceneAccess::lastApplyPlan(scene);
      LOKA_VERIFY(plan.hasPaintWork());
      LOKA_VERIFY(!plan.hasLayoutWork());
      LOKA_VERIFY(Access::onBoundaryApplyCalls(controller) > 0);
      LOKA_VERIFY(!Access::lastOnChangeRequiredLayout(controller));
      Access::PendingInvalidationSnapshot request;
      const bool hasRequest = Access::queryPendingInvalidation(controller, 0, request);
      Access::flushPendingInvalidations(controller);
      if (refused || !clearSurface)
        LOKA_VERIFY(Access::queuedPaintInvalidates(controller) == 1);
      else
        LOKA_VERIFY(Access::queuedPaintInvalidates(controller) == 0);
      LOKA_VERIFY(hasRequest);
      if (refused || !clearSurface)
      {
        LOKA_VERIFY(request.hwnd == rootHwnd && request.includeChildren);
      }
      else
      {
        LOKA_VERIFY(request.hwnd == (firstRect.top < secondRect.top ? first : second));
        LOKA_VERIFY(!request.fullWindow && !request.eraseBackground && !request.includeChildren);
        RECT client;
        LOKA_VERIFY(GetClientRect(request.hwnd, &client));
        LOKA_VERIFY(EqualRect(&request.rect, &client));
      }
      pumpMessages();
      UpdateWindow(rootHwnd);
      // The refused path must replay both surfaces after the root fills its
      // ground; the exact path must repaint A without requiring that replay.
      LOKA_VERIFY(Access::redrawStats(controller).rectSurfacePaintCount >= (refused || !clearSurface ? 2 : 1));
    }
    loka::core::resource::Image capture;
    LOKA_VERIFY(Access::captureWindowClientBitmap(rootHwnd, capture));
    HDC pixels = CreateCompatibleDC(NULL);
    LOKA_VERIFY(pixels != NULL);
    HGDIOBJ previous = SelectObject(pixels, static_cast<HBITMAP>(capture.nativeHandle()));
    LOKA_VERIFY(previous != NULL && previous != HGDI_ERROR);
    const COLORREF aPixel = GetPixel(pixels, aPoint.x, aPoint.y);
    const COLORREF bPixel = GetPixel(pixels, bPoint.x, bPoint.y);
    SelectObject(pixels, previous);
    DeleteDC(pixels);
    const Access::RedrawStats &stats = Access::redrawStats(controller);
    std::printf("#725 phase %d: A=%08lX B=%08lX window=%08lX; root paint=%d erase=%d surface paint=%d\n",
                phase,
                static_cast<unsigned long>(aPixel),
                static_cast<unsigned long>(bPixel),
                static_cast<unsigned long>(GetSysColor(COLOR_WINDOW)),
                stats.rootPaintCount,
                stats.rootEraseCount,
                stats.rectSurfacePaintCount);
    std::fflush(stdout);
    LOKA_VERIFY(bPixel == RGB(0, 0, 0) && "paint-only root delivery must preserve the sibling under ScrollView");
    const COLORREF expectedA = clearSurface ? (phase == 0 ? RGB(255, 255, 255) : RGB(0, 0, 0))
                                            : (phase == 0 ? RGB(0, 0, 0) : GetSysColor(COLOR_WINDOW));
    LOKA_VERIFY(aPixel == expectedA);
  }
  SceneAccess::unmount(scene);
  controller.drainNativeRetirements();
}

void testWin32PaintOnlyChangeUnderScrollViewKeepsSiblingPixels()
{
  exercisePaintOnlyChangeUnderScrollView(false);
}

void testWin32RefusedAnswerKeepsBroadFallback()
{
  exercisePaintOnlyChangeUnderScrollView(true);
}

void testWin32NonClearingSurfaceKeepsBroadFallback()
{
  exercisePaintOnlyChangeUnderScrollView(false, false);
  exercisePaintOnlyChangeUnderScrollView(false, false, true);
}

void testWin32PaintAnswerContracts()
{
  using namespace loka::app;
  using namespace loka::app::scene;
  typedef loka::dsl::testing::Win32ScenePlatformTestAccess Access;
  HWND root = CreateWindowExW(
      0, L"STATIC", L"paint-answer-contracts", WS_OVERLAPPED, 0, 0, 320, 240, NULL, NULL, GetModuleHandleW(NULL), NULL);
  LOKA_VERIFY(root != NULL);
  {
    Win32ScenePlatformController controller(root, loka::win32::Win32DisplayScale(96, loka::app::RailMetrics()));
    loka::core::PushStateTracker tracker;
    loka::core::MutableState<RectSurfaceModel> model((RectSurfaceModel()));
    loka::core::MutableState<loka::core::String> label(loka::core::String::Literal("before"));
    tracker.addState(&model);
    tracker.addState(&label);
    RectSurfaceProps props;
    props.model(&model).size(100, 60).clearBackground(true);
    RectSurfaceNode node(props);
    Win32RectSurfaceContext surface(&controller, root, 0, 0, 100, 60, &node);
    TextNode textNode((TextProps(&label)));
    Win32TextContext text(&controller, root, 0, 80, 100, 24, &textNode);
    PopupMenuNode popupNode((PopupMenuProps()));
    Win32PopupMenuContext popup(&controller, root, 0, 120, 100, 24, &popupNode);
    const PaintQuery query = {Win32RetirableContext::paintScope(), PLACEMENT_ELIGIBLE};
    const PaintQuery pending = {query.scope, PLACEMENT_PENDING};
    LOKA_VERIFY(surface.queryPaintDamage(query).reason == PAINT_REFUSED_HISTORY_UNKNOWN);
    LOKA_VERIFY(popup.queryPaintDamage(query).kind == PAINT_ANSWER_EXACT);
    LOKA_VERIFY(popup.queryPaintDamage(query).damage.width == 0);
    ShowWindow(root, SW_SHOWNOACTIVATE);
    Access::flushPendingInvalidations(controller);
    pumpMessages();
    UpdateWindow(root);
    // Establish a full-client presentation, independent of expose clipping.
    RedrawWindow(surface.paintHwnd(), NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    PaintAnswer answer = surface.queryPaintDamage(query);
    LOKA_VERIFY(answer.kind == PAINT_ANSWER_EXACT);
    LOKA_VERIFY(answer.damage.width == 0 && answer.damage.height == 0);
    RectSurfaceModel black;
    black.rectCount = 1;
    black.rects[0] = RectSprite(0, 0, 100, 60);
    {
      loka::core::StateTrackerGuard guard(&tracker);
      model.set(black);
      label.set(loka::core::String::Literal("after"));
    }
    answer = surface.queryPaintDamage(query);
    LOKA_VERIFY(answer.kind == PAINT_ANSWER_EXACT);
    LOKA_VERIFY(answer.damage.scope == query.scope);
    LOKA_VERIFY(answer.damage.x == 0 && answer.damage.y == 0);
    LOKA_VERIFY(answer.damage.width == 100 && answer.damage.height == 60);
    LOKA_VERIFY(answer.damage.coverage == PAINT_COVERAGE_PAINT_ONLY);
    LOKA_VERIFY(surface.queryPaintDamage(pending).reason == PAINT_REFUSED_PLACEMENT_UNSETTLED);
    LOKA_VERIFY(text.queryPaintDamage(query).kind == PAINT_ANSWER_NATIVE_SCHEDULED);
    // A read-only query must not consume the delivery fact.
    LOKA_VERIFY(text.queryPaintDamage(query).kind == PAINT_ANSWER_NATIVE_SCHEDULED);
    {
      loka::core::StateTrackerGuard guard(&tracker);
      label.set(loka::core::String::Literal("after"), true);
    }
    LOKA_VERIFY(text.queryPaintDamage(query).kind == PAINT_ANSWER_EXACT);
    LOKA_VERIFY(text.queryPaintDamage(query).damage.width == 0);
    {
      loka::core::StateTrackerGuard guard(&tracker);
      label.set(loka::core::String::Literal("later")); // Same length, different text.
    }
    LOKA_VERIFY(text.queryPaintDamage(query).kind == PAINT_ANSWER_NATIVE_SCHEDULED);
    const char *longText = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                           "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                           "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
                           "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
    {
      loka::core::StateTrackerGuard guard(&tracker);
      label.set(loka::core::String::Literal(longText));
    }
    LOKA_VERIFY(text.queryPaintDamage(query).kind == PAINT_ANSWER_NATIVE_SCHEDULED);
    {
      loka::core::StateTrackerGuard guard(&tracker);
      label.set(loka::core::String::Literal(longText), true);
    }
    // 256 characters cannot fit with a terminator: no unbounded read/allocation.
    LOKA_VERIFY(text.queryPaintDamage(query).kind == PAINT_ANSWER_NATIVE_SCHEDULED);
    Access::flushPendingInvalidations(controller);
    pumpMessages();
    RedrawWindow(surface.paintHwnd(), NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    LOKA_VERIFY(surface.queryPaintDamage(query).damage.width == 0);
    const RECT partial = {0, 0, 10, 10};
    RedrawWindow(surface.paintHwnd(), &partial, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    LOKA_VERIFY(surface.queryPaintDamage(query).reason == PAINT_REFUSED_HISTORY_UNKNOWN);
    RedrawWindow(surface.paintHwnd(), NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    LOKA_VERIFY(surface.queryPaintDamage(query).kind == PAINT_ANSWER_EXACT);
    // Its WM_ERASEBKGND cannot restore ground: a non-clearing change refuses.
    node.props.clearBackground(false);
    answer = surface.queryPaintDamage(query);
    LOKA_VERIFY(answer.kind == PAINT_ANSWER_REFUSED);
    LOKA_VERIFY(answer.reason == PAINT_REFUSED_UNSUPPORTED_KIND);
    node.props.clearBackground(true);
    surface.onPropsApplied();
    LOKA_VERIFY(surface.queryPaintDamage(query).reason == PAINT_REFUSED_HISTORY_UNKNOWN);
    surface.relayout(0, 0, 90, 60);
    // relayout may synchronously repaint; an explicit resize invalidates history.
    SendMessageW(surface.paintHwnd(), WM_SIZE, 0, MAKELPARAM(90, 60));
    LOKA_VERIFY(surface.queryPaintDamage(query).reason == PAINT_REFUSED_HISTORY_UNKNOWN);
    surface.onFactChanged(NODE_FACT_ATTACHED, NODE_FACT_DETACHED_RETAINED);
    LOKA_VERIFY(surface.queryPaintDamage(query).reason == PAINT_REFUSED_HISTORY_UNKNOWN);
    text.onFactChanged(NODE_FACT_ATTACHED, NODE_FACT_RETIRED);
    surface.onFactChanged(NODE_FACT_DETACHED_RETAINED, NODE_FACT_RETIRED);
    popup.onFactChanged(NODE_FACT_ATTACHED, NODE_FACT_RETIRED);
    LOKA_VERIFY(surface.queryPaintDamage(query).reason == PAINT_REFUSED_NO_CONTEXT);
    LOKA_VERIFY(text.queryPaintDamage(query).reason == PAINT_REFUSED_NO_CONTEXT);
    controller.drainNativeRetirements();
    // Foreign contexts cannot replace kinds that the paint visitor casts.
    RefusedNodeHandler foreign(NodeTypeToken<TextNode>());
    LOKA_VERIFY(!controller.registerNodeHandler(&foreign));
  }
  DestroyWindow(root);
}

void testWin32EditTextPaintDelivery()
{
  using namespace loka::app;
  using namespace loka::app::scene;
  HWND root = CreateWindowExW(
      0, L"STATIC", L"edit-delivery", WS_OVERLAPPED, 0, 0, 320, 240, NULL, NULL, GetModuleHandleW(NULL), NULL);
  LOKA_VERIFY(root != NULL);
  {
    Win32ScenePlatformController controller(root, loka::win32::Win32DisplayScale(96, loka::app::RailMetrics()));
    loka::core::PushStateTracker tracker;
    loka::core::MutableState<loka::core::String> value(loka::core::String::Literal("before"));
    loka::core::MutableState<loka::core::String> replacement(loka::core::String::Literal("foreign"));
    tracker.addState(&value);
    EditTextNode node((EditTextProps(loka::app::scene::WriteSeat<loka::core::String>(&value))));
    Win32EditTextContext context(&controller, root, 0, 0, 100, 24, &node);
    const PaintQuery query = {Win32RetirableContext::paintScope(), PLACEMENT_ELIGIBLE};
    {
      loka::core::StateTrackerGuard guard(&tracker);
      value.set(loka::core::String::Literal("before"), true);
    }
    PaintAnswer answer = context.queryPaintDamage(query);
    LOKA_VERIFY(answer.kind == PAINT_ANSWER_EXACT && answer.damage.width == 0 && answer.damage.height == 0);
    LOKA_VERIFY(answer.damage.scope == query.scope);
    {
      loka::core::StateTrackerGuard guard(&tracker);
      value.set(loka::core::String::Literal("after"));
    }
    LOKA_VERIFY(context.queryPaintDamage(query).kind == PAINT_ANSWER_NATIVE_SCHEDULED);
    LOKA_VERIFY(context.queryPaintDamage(query).kind == PAINT_ANSWER_NATIVE_SCHEDULED);
    // A native echo is already represented in the EDIT and owes no new damage.
    LOKA_VERIFY(SetWindowTextW(context.hwnd(), L"native"));
    {
      loka::core::StateTrackerGuard guard(&tracker);
      LOKA_VERIFY(context.handleCommand(MAKEWPARAM(0, EN_CHANGE), 0));
    }
    LOKA_VERIFY(value.get().equals(loka::core::String::Literal("native")));
    LOKA_VERIFY(context.queryPaintDamage(query).kind == PAINT_ANSWER_EXACT);
    node.props.text(loka::app::scene::WriteSeat<loka::core::String>(&replacement));
    LOKA_VERIFY(context.queryPaintDamage(query).reason == PAINT_REFUSED_PROPS_UNRECONCILED);
    node.props.text(loka::app::scene::WriteSeat<loka::core::String>(&value));
    context.onFactChanged(NODE_FACT_ATTACHED, NODE_FACT_DETACHED_RETAINED);
    LOKA_VERIFY(context.queryPaintDamage(query).reason == PAINT_REFUSED_HISTORY_UNKNOWN);
    context.onFactChanged(NODE_FACT_DETACHED_RETAINED, NODE_FACT_RETIRED);
    LOKA_VERIFY(context.queryPaintDamage(query).reason == PAINT_REFUSED_NO_CONTEXT);
    EditTextNode unboundNode((EditTextProps()));
    Win32EditTextContext unbound(&controller, root, 0, 30, 100, 24, &unboundNode);
    LOKA_VERIFY(unbound.queryPaintDamage(query).reason == PAINT_REFUSED_HISTORY_UNKNOWN);
    unbound.onFactChanged(NODE_FACT_ATTACHED, NODE_FACT_RETIRED);
    controller.drainNativeRetirements();
  }
  DestroyWindow(root);
}

void testWin32PopupMenuPaintDelivery()
{
  using namespace loka::app;
  using namespace loka::app::scene;
  HWND root = CreateWindowExW(
      0, L"STATIC", L"popup-delivery", WS_OVERLAPPED, 0, 0, 320, 240, NULL, NULL, GetModuleHandleW(NULL), NULL);
  LOKA_VERIFY(root != NULL);
  {
    Win32ScenePlatformController controller(root, loka::win32::Win32DisplayScale(96, loka::app::RailMetrics()));
    loka::core::PushStateTracker tracker;
    loka::core::MutableState<int> selection(0);
    loka::core::MutableState<int> replacement(1);
    loka::core::MutableState<bool> enabled(true);
    loka::core::MutableState<bool> replacementEnabled(false);
    tracker.addState(&selection);
    tracker.addState(&enabled);
    const char *items[] = {"one", "two"};
    PopupMenuProps props;
    props.items(items, 2).selectedIndex(loka::app::scene::WriteSeat<int>(&selection)).enabled(&enabled);
    PopupMenuNode node(props);
    Win32PopupMenuContext context(&controller, root, 0, 0, 100, 24, &node);
    const PaintQuery query = {Win32RetirableContext::paintScope(), PLACEMENT_ELIGIBLE};
    {
      loka::core::StateTrackerGuard guard(&tracker);
      selection.set(0, true);
    }
    PaintAnswer answer = context.queryPaintDamage(query);
    LOKA_VERIFY(answer.kind == PAINT_ANSWER_EXACT && answer.damage.width == 0 && answer.damage.height == 0);
    LOKA_VERIFY(answer.damage.scope == query.scope);
    {
      loka::core::StateTrackerGuard guard(&tracker);
      selection.set(1);
    }
    LOKA_VERIFY(context.queryPaintDamage(query).kind == PAINT_ANSWER_NATIVE_SCHEDULED);
    LOKA_VERIFY(SendMessageW(context.hwnd(), CB_GETCURSEL, 0, 0) == 1);
    LOKA_VERIFY(context.queryPaintDamage(query).kind == PAINT_ANSWER_NATIVE_SCHEDULED);
    {
      loka::core::StateTrackerGuard guard(&tracker);
      enabled.set(false);
    }
    LOKA_VERIFY(context.queryPaintDamage(query).kind == PAINT_ANSWER_NATIVE_SCHEDULED);
    const bool nativeEnabled = IsWindowEnabled(context.hwnd()) != FALSE;
    LOKA_VERIFY(!nativeEnabled);
    {
      loka::core::StateTrackerGuard guard(&tracker);
      enabled.set(false, true);
    }
    LOKA_VERIFY(context.queryPaintDamage(query).kind == PAINT_ANSWER_EXACT);
    {
      loka::core::StateTrackerGuard guard(&tracker);
      selection.set(-1);
    }
    LOKA_VERIFY(context.queryPaintDamage(query).kind == PAINT_ANSWER_NATIVE_SCHEDULED);
    LOKA_VERIFY(SendMessageW(context.hwnd(), CB_GETCURSEL, 0, 0) == CB_ERR);
    {
      loka::core::StateTrackerGuard guard(&tracker);
      selection.set(-1, true);
    }
    LOKA_VERIFY(context.queryPaintDamage(query).kind == PAINT_ANSWER_EXACT);
    node.props.selectedIndex(loka::app::scene::WriteSeat<int>(&replacement));
    LOKA_VERIFY(context.queryPaintDamage(query).reason == PAINT_REFUSED_PROPS_UNRECONCILED);
    node.props.selectedIndex(loka::app::scene::WriteSeat<int>(&selection)).enabled(&replacementEnabled);
    LOKA_VERIFY(context.queryPaintDamage(query).reason == PAINT_REFUSED_PROPS_UNRECONCILED);
    node.props.enabled(&enabled);
    context.onFactChanged(NODE_FACT_ATTACHED, NODE_FACT_DETACHED_RETAINED);
    LOKA_VERIFY(context.queryPaintDamage(query).reason == PAINT_REFUSED_HISTORY_UNKNOWN);
    context.onFactChanged(NODE_FACT_DETACHED_RETAINED, NODE_FACT_RETIRED);
    LOKA_VERIFY(context.queryPaintDamage(query).reason == PAINT_REFUSED_NO_CONTEXT);
    controller.drainNativeRetirements();
  }
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
  WindowAdmissionTestApp admission(window);
  admission.flush();
  HWND root = window.hwnd();
  LOKA_VERIFY(root != NULL);
  ShowWindow(root, SW_HIDE);
  {
    Win32ScenePlatformController controller(root, loka::win32::Win32DisplayScale(96, loka::app::RailMetrics()));
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
