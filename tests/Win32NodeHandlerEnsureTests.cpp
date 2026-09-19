#include "Win32NodeHandlerEnsureTests.hpp"
#include "support/TestVerify.hpp"
#include <cassert>
#include <cstdio>
#include <windows.h>
#include "Win32BuiltInSupport.hpp"
#include "Win32ScenePlatformController.hpp"
#include "app/nodes/Text.hpp"
#include "app/layout/FallbackControlMetrics.hpp"
#include "core/StateTracker.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/ScrollBar.hpp"
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
    Win32ScenePlatformController controller(root, loka::win32::Win32DisplayScale(96));
    RegisterWin32BuiltInSupport(controller);

    // -- Button: full contract via the hwnd accessor --
    loka::app::ButtonProps buttonProps;
    loka::app::ButtonNode button(buttonProps);

    loka::app::scene::LayoutState state;
    state.x = 10;
    state.y = 20;
    state.width = 100;
    state.height = 30;
    LOKA_VERIFY(controller.prepareProjectedLayout(&button, state));

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
    LOKA_VERIFY(controller.prepareProjectedLayout(&button, state));
    LOKA_VERIFY(button.getContext() == ctx && "re-ensure must reuse the existing context, not recreate it");
    assert(countChildWindows(root) == childrenAfterFirstEnsure &&
           "re-ensure must not materialize another native window");
    r = childRectInParent(ctx->hwnd(), root);
    assert(r.left == 40 && r.top == 50 && r.right - r.left == 120 && r.bottom - r.top == 40 &&
           "re-ensure must route through relayout so the window follows the requested geometry");

    const HFONT font96 = reinterpret_cast<HFONT>(
        SendMessageW(ctx->hwnd(), WM_GETFONT, 0, 0));
    LOKA_VERIFY(font96);
    controller.updateDisplayScale(loka::win32::Win32DisplayScale(192));
    LOKA_VERIFY(controller.prepareProjectedLayout(&button, state));
    const HFONT font192 = reinterpret_cast<HFONT>(
        SendMessageW(ctx->hwnd(), WM_GETFONT, 0, 0));
    LOKA_VERIFY(font192 && font192 != font96
                && "a DPI change must replace each native control's message font");
    controller.updateDisplayScale(loka::win32::Win32DisplayScale(192));
    LOKA_VERIFY(reinterpret_cast<HFONT>(
                    SendMessageW(ctx->hwnd(), WM_GETFONT, 0, 0)) == font192
                && "a matching DPI must retain the already-applied font");
    LOGFONTW releasedFont;
    ZeroMemory(&releasedFont, sizeof(releasedFont));
    LOKA_VERIFY(GetObjectW(font96, sizeof(releasedFont), &releasedFont) == 0
                && "the replaced controller-owned font must be released");
    r = childRectInParent(ctx->hwnd(), root);
    assert(r.left == 80 && r.top == 100
           && r.right - r.left == 240 && r.bottom - r.top == 80
           && "a DPI change must reproject the same logical child frame");
    controller.updateDisplayScale(loka::win32::Win32DisplayScale(96));
    LOKA_VERIFY(controller.prepareProjectedLayout(&button, state));
    LOKA_VERIFY(GetObjectW(font192, sizeof(releasedFont), &releasedFont) == 0
                && "each later scale replacement must release its prior font");

    // -- Text: same contract on a second node kind (no hwnd accessor; the
    // child-window census carries the not-recreated leg) --
    loka::app::TextProps textProps;
    loka::app::TextNode text(textProps);
    state.x = 5;
    state.y = 100;
    state.width = 200;
    state.height = 16;
    LOKA_VERIFY(controller.prepareProjectedLayout(&text, state));
    loka::app::scene::NodeContext *textCtx = text.getContext();
    assert(textCtx && "ensure must publish the created context through setContext");
    const int childrenWithText = countChildWindows(root);
    assert(childrenWithText == childrenAfterFirstEnsure + 1);
    LOKA_VERIFY(controller.prepareProjectedLayout(&text, state));
    LOKA_VERIFY(text.getContext() == textCtx);
    assert(countChildWindows(root) == childrenWithText);

    // -- ScrollBar: Win32 has no native context for it; the registered
    // refusal stub must answer (false, no context) without tripping the
    // registry-miss education assert -- a known unsupported kind is a typed
    // refusal, not an accident. Reaching this line in a Debug build IS the
    // no-abort discrimination.
    loka::app::ScrollBarProps scrollProps;
    loka::app::ScrollBarNode scrollBar(scrollProps);
    state.x = 5;
    state.y = 130;
    state.width = 120;
    state.height = 16;
    LOKA_VERIFY(!controller.prepareProjectedLayout(&scrollBar, state) &&
           "an unsupported kind must refuse, not project");
    LOKA_VERIFY(!scrollBar.getContext());
    assert(countChildWindows(root) == childrenWithText &&
           "a refusal must not materialize a native window");

    printf("  button ctx=%p reused, children stable at %d; text ctx reused; scrollbar refused\n",
           static_cast<void *>(ctx), childrenWithText);
    // Nodes leave scope before the controller: ~Node retires and releases the
    // contexts, which destroys their child windows while root is still alive.
  }
  DestroyWindow(root);
  printf("==== [testWin32NodeHandlerEnsureContract] PASSED ====\n");
}

namespace
{
  HWND projectFontText(Win32ScenePlatformController &controller, HWND root,
                       loka::app::TextNode &text, int width = 160)
  {
    loka::app::scene::LayoutState state;
    state.x = 0;
    state.y = 0;
    state.width = static_cast<short>(width);
    state.height = 20;
    LOKA_VERIFY(controller.prepareProjectedLayout(&text, state));
    HWND first = GetWindow(root, GW_CHILD);
    // CreateWindowExW places a new child at the bottom of the sibling Z-order.
    HWND child = first ? GetWindow(first, GW_HWNDLAST) : NULL;
    LOKA_VERIFY(child);
    return child;
  }

  HFONT readFont(HWND child, LOGFONTW &descriptor)
  {
    HFONT font = reinterpret_cast<HFONT>(SendMessageW(child, WM_GETFONT, 0, 0));
    LOKA_VERIFY(font);
    LOKA_VERIFY(GetObjectW(font, static_cast<int>(sizeof(descriptor)), &descriptor) != 0);
    return font;
  }

  int layoutFontText(Win32ScenePlatformController &controller, loka::app::TextNode &text)
  {
    loka::app::scene::LayoutState state;
    state.x = 0;
    state.y = 0;
    state.width = 160;
    state.height = 20;
    text.getContext()->layout(&controller, state);
    return state.height;
  }
}

void testWin32TextFontTable()
{
  using namespace loka::app;
  HWND root = CreateWindowExW(0, L"STATIC", L"font-host", WS_OVERLAPPED,
                              0, 0, 320, 240, NULL, NULL, GetModuleHandleW(NULL), NULL);
  LOKA_VERIFY(root);
  {
    Win32ScenePlatformController controller(root, loka::win32::Win32DisplayScale(96));
    RegisterWin32BuiltInSupport(controller);
    TextProps plainProps("Font table sample text with enough words to wrap over several lines.");
    TextNode plain(plainProps);
    HWND plainWindow = projectFontText(controller, root, plain);
    LOGFONTW defaultDescriptor;
    LOKA_VERIFY(readFont(plainWindow, defaultDescriptor) == controller.displayFont());
    LOKA_VERIFY(layoutFontText(controller, plain) == loka::app::layout::FallbackControlMetrics::kTextHeight);

    TextProps largeProps(plainProps);
    largeProps.textStyle_ = FontSize<24>();
    TextNode large(largeProps);
    HWND largeWindow = projectFontText(controller, root, large);
    LOGFONTW descriptor;
    HFONT font96 = readFont(largeWindow, descriptor);
    LOKA_VERIFY(font96 != controller.displayFont());
    LOKA_VERIFY(descriptor.lfHeight == -MulDiv(24, 96, 96));
    LOKA_VERIFY(layoutFontText(controller, large) > layoutFontText(controller, plain));

    TextProps boldProps(plainProps);
    boldProps.textStyle_ = Bold;
    TextNode bold(boldProps);
    HWND boldWindow = projectFontText(controller, root, bold);
    readFont(boldWindow, descriptor);
    LOKA_VERIFY(descriptor.lfWeight == FW_BOLD);
    LOKA_VERIFY(descriptor.lfHeight == defaultDescriptor.lfHeight);

    TextProps italicProps(plainProps);
    italicProps.textStyle_ = Italic;
    TextNode italic(italicProps);
    HWND italicWindow = projectFontText(controller, root, italic);
    readFont(italicWindow, descriptor);
    LOKA_VERIFY(descriptor.lfItalic != 0);

    // Same descriptor shares an admission-time handle, without lazy creation.
    TextNode same(largeProps);
    HWND sameWindow = projectFontText(controller, root, same);
    LOKA_VERIFY(readFont(sameWindow, descriptor) == font96);
    controller.updateDisplayScale(loka::win32::Win32DisplayScale(144));
    HFONT font144 = readFont(largeWindow, descriptor);
    LOKA_VERIFY(font144 != font96);
    LOKA_VERIFY(descriptor.lfHeight == -MulDiv(24, 144, 96));
    LOKA_VERIFY(GetObjectW(font96, static_cast<int>(sizeof(descriptor)), &descriptor) == 0);
    LOKA_VERIFY(readFont(plainWindow, descriptor) == controller.displayFont());
    controller.updateDisplayScale(loka::win32::Win32DisplayScale(144));
    LOKA_VERIFY(readFont(largeWindow, descriptor) == font144);

    // Retained apply changes the existing STATIC and requests the WM_SIZE door.
    MSG message;
    while (PeekMessageW(&message, root, WM_SIZE, WM_SIZE, PM_REMOVE)) {}
    large.props.textStyle_ = FontSize<12>() + Bold + Italic;
    large.getContext()->onPropsApplied();
    LOKA_VERIFY(readFont(largeWindow, descriptor) != font144);
    LOKA_VERIFY(descriptor.lfHeight == -MulDiv(12, 144, 96));
    LOKA_VERIFY(descriptor.lfWeight == FW_BOLD && descriptor.lfItalic != 0);
    LOKA_VERIFY(PeekMessageW(&message, root, WM_SIZE, WM_SIZE, PM_REMOVE));
    large.props.textStyle_ = TextStyle();
    large.getContext()->onPropsApplied();
    LOKA_VERIFY(readFont(largeWindow, descriptor) == controller.displayFont());

    // Layout is also the live-style projection path; it must not need props apply.
    large.props.textStyle_ = FontSize<24>();
    layoutFontText(controller, large);
    LOKA_VERIFY(readFont(largeWindow, descriptor) == font144);
    controller.updateDisplayScale(loka::win32::Win32DisplayScale(96));
    TextProps wrappedProps(plainProps);
    wrappedProps.blockStyle_.wrap(TEXT_WRAP_WORD);
    TextNode wrapped(wrappedProps);
    projectFontText(controller, root, wrapped);
    wrappedProps.textStyle_ = FontSize<24>();
    TextNode wrappedLarge(wrappedProps);
    projectFontText(controller, root, wrappedLarge);
    LOKA_VERIFY(layoutFontText(controller, wrappedLarge) > layoutFontText(controller, wrapped));

    loka::core::PushStateTracker tracker;
    loka::core::MutableState<TextStyle> liveStyle((FontSize<12>()));
    TextProps liveProps(plainProps);
    liveProps.textStyleState_ = &liveStyle;
    TextNode liveText(liveProps);
    HWND liveWindow = projectFontText(controller, root, liveText);
    readFont(liveWindow, descriptor);
    LOKA_VERIFY(descriptor.lfHeight == -MulDiv(12, 96, 96));
    {
      loka::core::StateTrackerGuard guard(&tracker);
      liveStyle.set(FontSize<24>() + Italic);
    }
    layoutFontText(controller, liveText);
    readFont(liveWindow, descriptor);
    LOKA_VERIFY(descriptor.lfHeight == -MulDiv(24, 96, 96));
    LOKA_VERIFY(descriptor.lfItalic != 0);
  }
  {
    Win32ScenePlatformController controller(root, loka::win32::Win32DisplayScale(144));
    RegisterWin32BuiltInSupport(controller);
    TextProps props("Admission at 150 percent");
    props.textStyle_ = FontSize<24>();
    TextNode text(props);
    HWND child = projectFontText(controller, root, text);
    LOGFONTW descriptor;
    readFont(child, descriptor);
    LOKA_VERIFY(descriptor.lfHeight == -MulDiv(24, 144, 96));
  }
  DestroyWindow(root);
}
