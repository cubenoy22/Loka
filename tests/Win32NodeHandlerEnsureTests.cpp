#include "app/nodes/controls/TextEditor.hpp"
#include "Win32NodeHandlerEnsureTests.hpp"
#include "support/TestVerify.hpp"
#include "support/RailTextLayoutFixture.hpp"
#include <cassert>
#include <cstdio>
#include <windows.h>
#include "Win32BuiltInSupport.hpp"
#include "Win32ScenePlatformController.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
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

  HWND nthChildWindow(HWND parent, int index)
  {
    HWND child = GetWindow(parent, GW_CHILD);
    for (int i = 0; child && i < index; ++i)
      child = GetWindow(child, GW_HWNDNEXT);
    return child;
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
  void verifyFixedColumnFitsAtBothScales()
  {
    using namespace loka::app;
    const int count = 3;
    const int height = 40 + count * layout::FallbackControlMetrics::kButtonHeight
                       + (count - 1) * layout::FallbackControlMetrics::kVerticalSpacing;
    const RailMetrics metrics[] = {RailMetrics(), loka::win32::DefaultRailMetrics()};
    for (int scaleIndex = 0; scaleIndex < 2; ++scaleIndex)
    {
      const loka::win32::Win32DisplayScale scale(96, metrics[scaleIndex]);
      HWND root = CreateWindowExW(0, L"STATIC", L"column-fit", WS_POPUP, 0, 0,
                                  scale.clientLengthToNative(257).px,
                                  scale.clientLengthToNative(height).px,
                                  NULL, NULL, GetModuleHandle(NULL), NULL);
      LOKA_VERIFY(root != NULL);
      {
        Win32ScenePlatformController controller(root, scale);
        RECT client;
        LOKA_VERIFY(GetClientRect(root, &client));
        LOKA_VERIFY(scale.clientCapacityToLu(client.right) == 257);
        LOKA_VERIFY(scale.clientCapacityToLu(client.bottom) == height);
        StackNode column((StackProps(STACK_AXIS_COLUMN)));
        ButtonNode *last = 0;
        for (int i = 0; i < count; ++i)
        {
          last = new ButtonNode(ButtonProps());
          column.addChild(last);
        }
        controller.onChange(&column, loka::app::scene::NODE_DIRTY_NONE, false);
        controller.relayoutNativeClientPixels(client.right, client.bottom);
        LOKA_VERIFY(countChildWindows(root) == count);
        Win32ButtonContext *context = static_cast<Win32ButtonContext *>(last->getContext());
        LOKA_VERIFY(context != 0);
        const RECT frame = childRectInParent(context->hwnd(), root);
        LOKA_VERIFY(frame.bottom == scale.projectEdge(height - 20));
        LOKA_VERIFY(frame.bottom <= client.bottom);
        controller.onChange(0, loka::app::scene::NODE_DIRTY_NONE, false);
      }
      LOKA_VERIFY(DestroyWindow(root));
    }
  }
  // Native-measurement twin of the Mac pin: the shared fixture owns the
  // composition; this rail independently checks GDI measurements and HWNDs.
  void verifyTextColumnExtents()
  {
    using namespace loka::app;
    const RailMetrics defaults = loka::win32::DefaultRailMetrics();
    const RailMetrics metrics[] = {RailMetrics(), defaults};
    const char *strings[] = {"First", "First\nSecond"};
    const wchar_t *wideStrings[] = {L"First", L"First\nSecond"};
    for (int scaleIndex = 0; scaleIndex < 2; ++scaleIndex)
      for (int boxed = 0; boxed < 2; ++boxed)
        for (int sample = 0; sample < 2; ++sample)
        {
          const loka::win32::Win32DisplayScale scale(96, metrics[scaleIndex]);
          HWND root = CreateWindowExW(0, L"STATIC", L"text-column", WS_POPUP, 0, 0,
                                      scale.clientLengthToNative(340).px,
                                      scale.clientLengthToNative(250).px,
                                      NULL, NULL, GetModuleHandleW(NULL), NULL);
          LOKA_VERIFY(root != NULL);
          {
            Win32ScenePlatformController controller(root, scale);
            RailTextLayoutFixture fixture(boxed != 0, strings[sample]);
            controller.onChange(&fixture.column, loka::app::scene::NODE_DIRTY_NONE, false);
            controller.relayout(0, 0);
            RECT client;
            LOKA_VERIFY(GetClientRect(root, &client));
            LOKA_VERIFY(scale.clientCapacityToLu(client.bottom) == 250);
            LOKA_VERIFY(countChildWindows(root) == (boxed ? 5 : 3));
            // Text contexts publish no HWND accessor; walk the root's children in
            // creation order like the Mac twin walks subviews: page, caption,
            // [badge], button. The Button context does expose its HWND.
            // GetWindow(GW_CHILD) may list either creation order or z-order
            // (newest first); the Button's published HWND anchors which one.
            const int count = boxed ? 5 : 3;
            const int buttonIndex = boxed ? 3 : 2;
            Win32ButtonContext *buttonContext = static_cast<Win32ButtonContext *>(fixture.button->getContext());
            LOKA_VERIFY(buttonContext && buttonContext->hwnd());
            HWND button = buttonContext->hwnd();
            HWND page = 0;
            HWND caption = 0;
            if (nthChildWindow(root, buttonIndex) == button)
            {
              page = nthChildWindow(root, 0);
              caption = nthChildWindow(root, 1);
            }
            else if (nthChildWindow(root, count - 1 - buttonIndex) == button)
            {
              page = nthChildWindow(root, count - 1);
              caption = nthChildWindow(root, count - 2);
            }
            LOKA_VERIFY(page && caption && page != caption && caption != button);
            HDC dc = GetDC(page);
            LOKA_VERIFY(dc != NULL);
            HGDIOBJ previous = SelectObject(dc, controller.textFont(FontSize<18>()));
            TEXTMETRICW font;
            LOKA_VERIFY(GetTextMetricsW(dc, &font));
            const UINT flags = DT_LEFT | DT_NOPREFIX | DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL;
            RECT oneLine = {0, 0, scale.nativeLength(0, 300).px, 0};
            LOKA_VERIFY(DrawTextW(dc, L"First", -1, &oneLine, flags) > 0);
            RECT measured = {0, 0, scale.nativeLength(0, 300).px, 0};
            LOKA_VERIFY(DrawTextW(dc, wideStrings[sample], -1, &measured, flags) > 0);
            SelectObject(dc, previous);
            ReleaseDC(page, dc);
            const int nativeHeight = measured.bottom - measured.top;
            const int singleHeight = oneLine.bottom - oneLine.top;
            LOKA_VERIFY(nativeHeight == (sample + 1) * singleHeight);
            const int fontHeight = scale.measurementToLu(font.tmHeight + font.tmExternalLeading);
            const int minimum = fontHeight > 20 ? fontHeight : 20;
            const int padded = scale.measurementToLu(nativeHeight) + 8;
            const int textHeight = padded > minimum ? padded : minimum;
            const int captionY = boxed ? 190 : 20 + textHeight + 12;
            const int buttonY = captionY + 20 + 12;
            const RECT pageFrame = childRectInParent(page, root);
            const RECT expectedPage = scale.projectFrame(loka::core::Frame(20, 20, 300, textHeight)).r;
            LOKA_VERIFY(EqualRect(&pageFrame, &expectedPage));
            LOKA_VERIFY(childRectInParent(caption, root).top == scale.projectEdge(captionY));
            const RECT buttonFrame = childRectInParent(button, root);
            LOKA_VERIFY(buttonFrame.top == scale.projectEdge(buttonY));
            LOKA_VERIFY(buttonFrame.bottom == scale.projectEdge(buttonY + 32));
            if (boxed)
              LOKA_VERIFY(buttonY + 32 == 254);
            std::printf("  Win32 text extent: space=%d/%d boxed=%d lines=%d nativeText=%d "
                        "textLu=%d buttonLu=%d..%d framePx=%ld..%ld clientPx=%ld\n",
                        metrics[scaleIndex].spaceScale.num, metrics[scaleIndex].spaceScale.den,
                        boxed, sample + 1, nativeHeight, textHeight, buttonY, buttonY + 32,
                        buttonFrame.top, buttonFrame.bottom, client.bottom);
            controller.onChange(0, loka::app::scene::NODE_DIRTY_NONE, false);
          }
          LOKA_VERIFY(DestroyWindow(root));
        }
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
  verifyFixedColumnFitsAtBothScales();
  verifyTextColumnExtents();
  HWND root = CreateWindowExW(
      0, L"STATIC", L"ensure-host", WS_OVERLAPPED, 0, 0, 320, 240, NULL, NULL, GetModuleHandle(NULL), NULL);
  assert(root);
  {
    Win32ScenePlatformController controller(root, loka::win32::Win32DisplayScale(96, loka::app::RailMetrics()));
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
    LOKA_VERIFY(ctx->hwnd() && IsWindow(ctx->hwnd()));
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
    LOKA_VERIFY(countChildWindows(root) == childrenAfterFirstEnsure &&
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
    LOKA_VERIFY(countChildWindows(root) == childrenWithText);

    // -- ScrollBar: Win32 has no native context for it; the registered
    // refusal stub must answer (false, no context) without tripping the
    // registry-miss education assert -- a known unsupported kind is a typed
    // refusal, not an accident. Reaching this line in a Debug build IS the
    // no-abort discrimination.
    loka::app::TextEditorNode editor((loka::app::TextEditorProps()));
    LOKA_VERIFY(!controller.prepareProjectedLayout(&editor, state));
    LOKA_VERIFY(!editor.getContext());
    loka::app::ScrollBarProps scrollProps;
    loka::app::ScrollBarNode scrollBar(scrollProps);
    state.x = 5;
    state.y = 130;
    state.width = 120;
    state.height = 16;
    LOKA_VERIFY(!controller.prepareProjectedLayout(&scrollBar, state) &&
           "an unsupported kind must refuse, not project");
    LOKA_VERIFY(!scrollBar.getContext());
    LOKA_VERIFY(countChildWindows(root) == childrenWithText &&
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
    Win32ScenePlatformController controller(root, loka::win32::Win32DisplayScale(96, loka::app::RailMetrics()));
    RegisterWin32BuiltInSupport(controller);
    TextProps plainProps("Font table sample text with enough words to wrap over several lines.");
    TextNode plain(plainProps);
    HWND plainWindow = projectFontText(controller, root, plain);
    LOGFONTW defaultDescriptor;
    LOKA_VERIFY(readFont(plainWindow, defaultDescriptor) == controller.displayFont());
    LOKA_VERIFY(layoutFontText(controller, plain) == loka::app::layout::FallbackControlMetrics::kTextHeight);

    TextProps largeProps(plainProps);
    largeProps.textStyle_ = FontSize<24>();
    TextNode largeMetrics(largeProps);
    HWND largeWindow = projectFontText(controller, root, largeMetrics);
    LOGFONTW descriptor;
    HFONT font96 = readFont(largeWindow, descriptor);
    LOKA_VERIFY(font96 != controller.displayFont());
    LOKA_VERIFY(descriptor.lfHeight == -MulDiv(24, 96, 96));
    LOKA_VERIFY(layoutFontText(controller, largeMetrics) > layoutFontText(controller, plain));

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
    largeMetrics.props.textStyle_ = FontSize<12>() + Bold + Italic;
    largeMetrics.getContext()->onPropsApplied();
    LOKA_VERIFY(readFont(largeWindow, descriptor) != font144);
    LOKA_VERIFY(descriptor.lfHeight == -MulDiv(12, 144, 96));
    LOKA_VERIFY(descriptor.lfWeight == FW_BOLD && descriptor.lfItalic != 0);
    LOKA_VERIFY(PeekMessageW(&message, root, WM_SIZE, WM_SIZE, PM_REMOVE));
    largeMetrics.props.textStyle_ = TextStyle();
    largeMetrics.getContext()->onPropsApplied();
    LOKA_VERIFY(readFont(largeWindow, descriptor) == controller.displayFont());

    // Layout is also the live-style projection path; it must not need props apply.
    largeMetrics.props.textStyle_ = FontSize<24>();
    layoutFontText(controller, largeMetrics);
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
    Win32ScenePlatformController controller(root, loka::win32::Win32DisplayScale(144, loka::app::RailMetrics()));
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


#include "context/Win32AttributedTextContext.hpp"
#include "support/LokaAllocFailure.hpp"
#include <climits>

namespace loka
{
  namespace testing
  {
    class Win32AttributedTextAccess
    {
    public:
      static const Win32AttributedTextTable &table(const Win32AttributedTextContext &context)
      {
        return context.table_;
      }
      static bool known(const Win32AttributedTextContext &context)
      {
        return context.presented_.isKnown();
      }
      static void draw(Win32AttributedTextContext &context, HDC dc, const RECT &rect)
      {
        context.draw(dc, rect);
      }
      static HFONT font(const Win32AttributedTextTable &table, std::size_t i)
      {
        return table.fonts_[i];
      }
      static WCHAR unit(const Win32AttributedTextTable &table, std::size_t i)
      {
        return table.units_[i];
      }
      static std::size_t unitCount(const Win32AttributedTextTable &table)
      {
        return table.units_.size();
      }
      static const loka::app::TextLineMetrics &metrics(const Win32AttributedTextTable &table, std::size_t i)
      {
        return table.metrics_[i];
      }
      static Win32AttributedTextContext *fromWindow(HWND hwnd)
      {
        return reinterpret_cast<Win32AttributedTextContext *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
      }
    };
  } // namespace testing
} // namespace loka
namespace
{
  typedef loka::testing::Win32AttributedTextAccess AttributedAccess;
  loka::app::scene::LayoutState attributedSeat(short width)
  {
    loka::app::scene::LayoutState state;
    state.x = 1;
    state.y = 3;
    state.width = width;
    state.height = 80;
    return state;
  }
  int measureSpan(HWND hwnd, HFONT font, const wchar_t *text, int count)
  {
    HDC dc = GetDC(hwnd);
    LOKA_VERIFY(dc);
    HGDIOBJ previous = SelectObject(dc, font);
    SIZE size;
    LOKA_VERIFY(GetTextExtentExPointW(dc, text, count, INT_MAX, NULL, NULL, &size));
    SelectObject(dc, previous);
    ReleaseDC(hwnd, dc);
    return size.cx;
  }
  HWND attributedHost()
  {
    HWND root = CreateWindowExW(
        0, L"STATIC", L"attributed-host", WS_POPUP, 0, 0, 600, 400, NULL, NULL, GetModuleHandleW(NULL), NULL);
    LOKA_VERIFY(root);
    return root;
  }
} // namespace
void testWin32AttributedTextPerRunProjection()
{
  using namespace loka::app;
  using namespace loka::app::scene;
  HWND root = attributedHost();
  {
    const loka::win32::Win32DisplayScale scale(144, RailMetrics());
    Win32ScenePlatformController controller(root, scale);
    LOKA_VERIFY(controller.textShaping() == PER_RUN);
    RefusedNodeHandler replacement(NodeTypeToken<AttributedTextNode>());
    LOKA_VERIFY(!controller.registerNodeHandler(&replacement));
    const AttributedString value = Styled("a ab", FontSize<12>() + Bold) + Styled("cd", FontSize<24>() + Italic);
    AttributedTextNode node((AttributedText(value) + BlockStyle().wrap(TEXT_WRAP_WORD)).props);
    const int wordWidth = measureSpan(root, controller.textFont(FontSize<12>() + Bold), L"ab", 2)
                          + measureSpan(root, controller.textFont(FontSize<24>() + Italic), L"cd", 2);
    // Choose odd logical width at odd origin: origin-free rounding differs.
    short width = static_cast<short>(scale.measurementToLu(wordWidth) + 2);
    if (!(width & 1))
      ++width;
    LayoutState state = attributedSeat(width);
    LOKA_VERIFY(controller.prepareProjectedLayout(&node, state));
    Win32AttributedTextContext *context = static_cast<Win32AttributedTextContext *>(node.getContext());
    LOKA_VERIFY(context && context->paintHwnd());
    context->layout(&controller, state);
    const Win32AttributedTextTable &table = AttributedAccess::table(*context);
    LOKA_VERIFY(table.valid() && table.lines().lineCount() == 2);
    LOKA_VERIFY(table.lines().line(1).fragmentCount == 2);
    const TextLineRecord &mixed = table.lines().line(1);
    const TextLineMetrics &smallMetrics = AttributedAccess::metrics(table, 0);
    const TextLineMetrics &largeMetrics = AttributedAccess::metrics(table, 1);
    LOKA_VERIFY(mixed.metrics.ascent == (smallMetrics.ascent > largeMetrics.ascent ? smallMetrics.ascent : largeMetrics.ascent));
    LOKA_VERIFY(mixed.metrics.descent == (smallMetrics.descent > largeMetrics.descent ? smallMetrics.descent : largeMetrics.descent));
    LOKA_VERIFY(mixed.width == wordWidth);
    RECT native = childRectInParent(context->paintHwnd(), root);
    LOKA_VERIFY(native.left == scale.projectEdge(1));
    LOKA_VERIFY(native.right - native.left == scale.nativeLength(1, 1 + width).px);
    LOKA_VERIFY(scale.nativeLength(1, 1 + width).px != scale.projectLength(width));
    const int expectedHeight =
        scale.measurementToLu(smallMetrics.ascent + smallMetrics.descent + smallMetrics.leading)
        + scale.measurementToLu(mixed.metrics.ascent + mixed.metrics.descent + mixed.metrics.leading);
    LOKA_VERIFY(state.height == expectedHeight);
    HWND child = context->paintHwnd();
    state = attributedSeat(width * 3);
    LOKA_VERIFY(controller.prepareProjectedLayout(&node, state));
    LOKA_VERIFY(node.getContext() == context && context->paintHwnd() == child);
    context->layout(&controller, state);
    LOKA_VERIFY(table.lines().lineCount() == 1);

    // Rendering into a deterministic memory surface discriminates erase,
    // partial history and saved GDI attributes without requiring a visible host.
    HDC windowDC = GetDC(root);
    HDC dc = CreateCompatibleDC(windowDC);
    HBITMAP bitmap = CreateCompatibleBitmap(windowDC, 400, 200);
    LOKA_VERIFY(dc && bitmap);
    HGDIOBJ previous = SelectObject(dc, bitmap);
    RECT rect = {0, 0, 400, 200};
    IntersectClipRect(dc, 0, 0, 400, 200);
    SetTextAlign(dc, TA_RIGHT | TA_TOP);
    SetBkMode(dc, OPAQUE);
    AttributedAccess::draw(*context, dc, rect);
    LOKA_VERIFY(GetTextAlign(dc) == (TA_RIGHT | TA_TOP) && GetBkMode(dc) == OPAQUE);
    LOKA_VERIFY(AttributedAccess::known(*context));
    bool ink = false;
    for (int y = 0; y < 80; ++y)
      for (int x = 0; x < 200; ++x)
        if (GetPixel(dc, x, y) != RGB(255, 255, 255))
          ink = true;
    LOKA_VERIFY(ink);
    const PaintQuery query = {Win32RetirableContext::paintScope(), PLACEMENT_ELIGIBLE};
    LOKA_VERIFY(context->queryPaintDamage(query).kind == PAINT_ANSWER_EXACT);
    IntersectClipRect(dc, 0, 0, 10, 10);
    AttributedAccess::draw(*context, dc, rect);
    LOKA_VERIFY(!AttributedAccess::known(*context));
    SelectClipRgn(dc, NULL);
    IntersectClipRect(dc, 0, 0, 400, 200);
    node.props = AttributedTextProps(AttributedString());
    context->onPropsApplied();
    state = attributedSeat(width);
    context->layout(&controller, state);
    AttributedAccess::draw(*context, dc, rect);
    for (int y = 0; y < 80; ++y)
      for (int x = 0; x < 200; ++x)
        LOKA_VERIFY(GetPixel(dc, x, y) == RGB(255, 255, 255));
    SelectObject(dc, previous);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(root, windowDC);

    node.props = AttributedTextProps(Styled("\xef\xbc\xa1\xf0\x9f\x98\x80", FontSize<12>()));
    context->onPropsApplied();
    state = attributedSeat(width);
    context->layout(&controller, state);
    LOKA_VERIFY(AttributedAccess::unitCount(table) == 3);
    LOKA_VERIFY(AttributedAccess::unit(table, 0) == 0xff21);
    LOKA_VERIFY(AttributedAccess::unit(table, 1) == 0xd83d && AttributedAccess::unit(table, 2) == 0xde00);
    node.props.blockStyle_.wrap(TEXT_WRAP_CHAR);
    state = attributedSeat(1);
    context->layout(&controller, state);
    LOKA_VERIFY(table.lines().lineCount() == 2);
    const TextFragment &pair = table.lines().fragment(table.lines().line(1).firstFragment);
    LOKA_VERIFY(pair.end - pair.start == 2);
    context->onFactChanged(NODE_FACT_ATTACHED, NODE_FACT_DETACHED_RETAINED);
    LOKA_VERIFY(table.valid() && IsWindow(child));
    LOKA_VERIFY(!(GetWindowLongPtrW(child, GWL_STYLE) & WS_VISIBLE));
    // A hidden update is rebuilt from current props and survives reveal.
    node.props = AttributedTextProps(value);
    context->onPropsApplied();
    state = attributedSeat(width);
    context->layout(&controller, state);
    context->onFactChanged(NODE_FACT_DETACHED_RETAINED, NODE_FACT_ATTACHED);
    LOKA_VERIFY(table.valid() && table.value() == value);
    LOKA_VERIFY(GetWindowLongPtrW(child, GWL_STYLE) & WS_VISIBLE);
    context->onFactChanged(NODE_FACT_ATTACHED, NODE_FACT_RETIRED);
    LOKA_VERIFY(!table.valid() && !context->paintHwnd());
    LOKA_VERIFY(IsWindow(child));
    controller.drainNativeRetirements();
    LOKA_VERIFY(!IsWindow(child));
  }
  LOKA_VERIFY(DestroyWindow(root));
}

void testWin32AttributedTextAllocationFailure()
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace loka::core::testing;
  HWND root = attributedHost();
  // The backend is installed across the entire allocation-balanced region.
  failLokaAllocRaw("Win32AttributedText", "Context", 1);
  {
    Win32ScenePlatformController controller(root, loka::win32::Win32DisplayScale(96, loka::app::RailMetrics()));
    AttributedTextNode node((AttributedTextProps(Styled("sample", Bold))));
    LayoutState state = attributedSeat(100);
    LOKA_VERIFY(!controller.prepareProjectedLayout(&node, state));
    LOKA_VERIFY(!node.getContext());
    LOKA_VERIFY(controller.prepareProjectedLayout(&node, state));
    Win32AttributedTextContext *context = static_cast<Win32AttributedTextContext *>(node.getContext());
    context->layout(&controller, state);
    LOKA_VERIFY(AttributedAccess::table(*context).valid());
    failLokaAllocRaw("Win32AttributedText", "Break", 1);
    context->layout(&controller, state);
    LOKA_VERIFY(!AttributedAccess::table(*context).valid() && !AttributedAccess::known(*context));
    const PaintQuery query = {Win32RetirableContext::paintScope(), PLACEMENT_ELIGIBLE};
    LOKA_VERIFY(context->queryPaintDamage(query).kind == PAINT_ANSWER_REFUSED);
    context->layout(&controller, state);
    LOKA_VERIFY(AttributedAccess::table(*context).valid());
  }
  LOKA_VERIFY(lokaAllocRawLive() == 0);
  allowLokaAllocRaw();
  LOKA_VERIFY(DestroyWindow(root));
}

void testWin32AttributedTextLiveDpiChange()
{
  using namespace loka::app;
  using namespace loka::app::scene;
  // Same shape as the Text DPI pin above: the controller owns the display
  // scale, updateDisplayScale is the door WM_DPICHANGED reaches (pinned in
  // Win32WindowClientSizeTests), and the broadcast must revoke every borrowed
  // HFONT before the old generation is deleted.
  HWND root = attributedHost();
  {
    Win32ScenePlatformController controller(root, loka::win32::Win32DisplayScale(96, loka::win32::DefaultRailMetrics()));
    AttributedTextNode node((AttributedTextProps(Styled("var x = ", FontSize<12>() + Bold) + Styled("1;", FontSize<24>() + Italic))));
    LayoutState state = attributedSeat(200);
    LOKA_VERIFY(controller.prepareProjectedLayout(&node, state));
    HWND child = FindWindowExW(root, NULL, L"LOKA_ATTRIBUTED_TEXT", NULL);
    LOKA_VERIFY(child);
    Win32AttributedTextContext *context = AttributedAccess::fromWindow(child);
    LOKA_VERIFY(context && node.getContext() == context);
    context->layout(&controller, state);
    const Win32AttributedTextTable &table = AttributedAccess::table(*context);
    LOKA_VERIFY(table.valid());
    const HFONT oldFont = AttributedAccess::font(table, 0);
    LOGFONTW descriptor;
    LOKA_VERIFY(GetObjectW(oldFont, sizeof(descriptor), &descriptor));
    // Generation change: the table is revoked before the old handles die.
    controller.updateDisplayScale(loka::win32::Win32DisplayScale(144, loka::win32::DefaultRailMetrics()));
    LOKA_VERIFY(!table.valid() && !AttributedAccess::known(*context));
    LOKA_VERIFY(GetObjectW(oldFont, sizeof(descriptor), &descriptor) == 0);
    // The next layout rebuilds against the new generation, never the dead handle.
    context->layout(&controller, state);
    LOKA_VERIFY(table.valid());
    LOKA_VERIFY(AttributedAccess::font(table, 0) != oldFont);
    LOKA_VERIFY(GetObjectW(AttributedAccess::font(table, 0), sizeof(descriptor), &descriptor));
    const loka::win32::Win32DisplayScale scale144(144, loka::win32::DefaultRailMetrics());
    LOKA_VERIFY(descriptor.lfHeight == -scale144.fontHeightToNative(12));
    LOKA_VERIFY(AttributedAccess::fromWindow(child) == context);
  }
}

#include "support/PropsReconciliation.hpp"
#include "testing/Win32ScenePlatformTestAccess.hpp"

void testWin32AttributedTextPaintRouting()
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace PropsReconciliationSupport;
  typedef loka::dsl::testing::Win32ScenePlatformTestAccess Access;
  HWND rootWindow = attributedHost();
  {
    Win32ScenePlatformController controller(rootWindow, loka::win32::Win32DisplayScale(96, loka::app::RailMetrics()));
    AttributedText declaration(Styled("var x = ", Bold) + Styled("1;", Italic));
    Scene scene((Boundary<Tree<AttributedText> >(Props<AttributedText>(&declaration))));
    scene.mount(&controller);
    loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
    settle(scene);
    BoundaryNode *boundary = root(scene);
    LOKA_VERIFY(boundary);
    controller.onChange(boundary, NODE_DIRTY_NONE, false);
    controller.relayout(320, 240);
    AttributedTextNode *node = boundary->childrenHead()->asAttributedTextNode();
    LOKA_VERIFY(node && node->getContext());
    Win32AttributedTextContext *context = static_cast<Win32AttributedTextContext *>(node->getContext());
    ShowWindow(rootWindow, SW_SHOWNOACTIVATE);
    Access::flushPendingInvalidations(controller);
    RedrawWindow(context->paintHwnd(), NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    LOKA_VERIFY(AttributedAccess::known(*context));
    BoundaryLocalApplyInfo info;
    info.paintKind = LOCAL_APPLY_PAINT_GENERIC;
    PlatformApplyPlan plan;
    plan.paintKind = PlatformApplyPlan::PAINT_LOCAL;
    plan.setPrimaryRoot(boundary);
    Access::resetRedrawStats(controller);
    controller.onBoundaryApply(boundary, boundary, info, plan);
    LOKA_VERIFY(Access::onBoundaryApplyCalls(controller) == 1);
    LOKA_VERIFY(Access::queuedPaintInvalidates(controller) == 0);
    // Positive control: unknown presentation must take the broad fallback.
    SendMessageW(context->paintHwnd(), WM_SIZE, 0, 0);
    controller.onBoundaryApply(boundary, boundary, info, plan);
    LOKA_VERIFY(Access::queuedPaintInvalidates(controller) == 1);
    // A props-only update schedules layout; it must not remain an empty cache.
    MSG message;
    while (PeekMessageW(&message, rootWindow, WM_SIZE, WM_SIZE, PM_REMOVE))
    {
    }
    node->props = AttributedTextProps(Styled("changed", FontSize<24>()));
    context->onPropsApplied();
    LOKA_VERIFY(PeekMessageW(&message, rootWindow, WM_SIZE, WM_SIZE, PM_REMOVE));
    controller.relayout(320, 240);
    LOKA_VERIFY(AttributedAccess::table(*context).valid());
    LOKA_VERIFY(AttributedAccess::table(*context).value() == node->props.text_->get());
    loka::dsl::testing::SceneTestAccess::unmount(scene);
  }
  LOKA_VERIFY(DestroyWindow(rootWindow));
}
