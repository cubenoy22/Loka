#include "Win32NodeHandlerEnsureTests.hpp"
#include <cassert>
#include <cstdio>
#include <windows.h>
#include "Win32BuiltInSupport.hpp"
#include "Win32ScenePlatformController.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/controls/Button.hpp"
#include "context/Win32ButtonContext.hpp"

namespace
{
  BOOL CALLBACK CountChildThunk(HWND, LPARAM lParam)
  {
    ++*reinterpret_cast<int *>(lParam);
    return TRUE;
  }

  int countChildWindows(HWND root)
  {
    int count = 0;
    EnumChildWindows(root, CountChildThunk, reinterpret_cast<LPARAM>(&count));
    return count;
  }

  RECT childRectInParent(HWND child, HWND parent)
  {
    RECT r;
    GetWindowRect(child, &r);
    POINT tl = {r.left, r.top};
    POINT br = {r.right, r.bottom};
    ScreenToClient(parent, &tl);
    ScreenToClient(parent, &br);
    RECT out = {tl.x, tl.y, br.x, br.y};
    return out;
  }
} // namespace

// Characterization for the per-cell ensure contract (#7 R1, moved out of
// Win32NodeContextMapper): a registered handler must (a) publish the created
// context through setContext, (b) reuse an existing context instead of
// materializing a second native window, and (c) route repeat ensures through
// the relayout path so the window follows the requested geometry. The
// attach-time lifecycle read is not discriminable headless (every Win32
// context creates its window WS_VISIBLE, and the read's distinct consumer is
// OpenFileDialog's presentIfNeeded, which cannot run in a test); that leg is
// covered by review plus the rig's Tutorial capture comparison.
void testWin32NodeHandlerEnsureContract()
{
  printf("\n==== [testWin32NodeHandlerEnsureContract] start ====\n");
  HWND root = CreateWindowExW(
      0, L"STATIC", L"ensure-host", WS_OVERLAPPED, 0, 0, 320, 240, NULL, NULL, GetModuleHandle(NULL), NULL);
  assert(root);
  {
    Win32ScenePlatformController controller(root);
    RegisterWin32BuiltInSupport(controller);

    // -- Button: full contract via the hwnd accessor --
    loka::app::ButtonProps buttonProps;
    loka::app::ButtonNode button(buttonProps);

    loka::app::scene::LayoutState state;
    state.x = 10;
    state.y = 20;
    state.width = 100;
    state.height = 30;
    assert(controller.prepareProjectedLayout(&button, state));

    Win32ButtonContext *ctx = static_cast<Win32ButtonContext *>(button.getContext());
    assert(ctx && "ensure must publish the created context through setContext");
    assert(ctx->hwnd() && IsWindow(ctx->hwnd()));
    RECT r = childRectInParent(ctx->hwnd(), root);
    assert(r.left == 10 && r.top == 20 && r.right - r.left == 100 && r.bottom - r.top == 30);
    const int childrenAfterFirstEnsure = countChildWindows(root);
    assert(childrenAfterFirstEnsure >= 1);

    // Second ensure with new geometry: same context, same window population,
    // window moved by the relayout path -- not recreated.
    state.x = 40;
    state.y = 50;
    state.width = 120;
    state.height = 40;
    assert(controller.prepareProjectedLayout(&button, state));
    assert(button.getContext() == ctx && "re-ensure must reuse the existing context, not recreate it");
    assert(countChildWindows(root) == childrenAfterFirstEnsure &&
           "re-ensure must not materialize another native window");
    r = childRectInParent(ctx->hwnd(), root);
    assert(r.left == 40 && r.top == 50 && r.right - r.left == 120 && r.bottom - r.top == 40 &&
           "re-ensure must route through relayout so the window follows the requested geometry");

    // -- Text: same contract on a second node kind (no hwnd accessor; the
    // child-window census carries the not-recreated leg) --
    loka::app::TextProps textProps;
    loka::app::TextNode text(textProps);
    state.x = 5;
    state.y = 100;
    state.width = 200;
    state.height = 16;
    assert(controller.prepareProjectedLayout(&text, state));
    loka::app::scene::NodeContext *textCtx = text.getContext();
    assert(textCtx && "ensure must publish the created context through setContext");
    const int childrenWithText = countChildWindows(root);
    assert(childrenWithText == childrenAfterFirstEnsure + 1);
    assert(controller.prepareProjectedLayout(&text, state));
    assert(text.getContext() == textCtx);
    assert(countChildWindows(root) == childrenWithText);

    printf("  button ctx=%p reused, children stable at %d; text ctx reused\n", static_cast<void *>(ctx),
           childrenWithText);
    // Nodes leave scope before the controller: ~Node retires and releases the
    // contexts, which destroys their child windows while root is still alive.
  }
  DestroyWindow(root);
  printf("==== [testWin32NodeHandlerEnsureContract] PASSED ====\n");
}
