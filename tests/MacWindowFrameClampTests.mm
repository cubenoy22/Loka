#include "MacWindowFrameClampTests.hpp"
#include "support/TestVerify.hpp"

#include <AppKit/AppKit.h>

#include "MacWindow.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "testing/MacWindowTestAccess.hpp"

namespace
{
  typedef loka::dsl::testing::MacWindowTestAccess NativeAccess;

  void verifyPlacedFrame(MacWindow &window)
  {
    NSWindow *native = (NSWindow *)NativeAccess::nativeWindow(window);
    LOKA_VERIFY(native != nil);
    LOKA_VERIFY([native isVisible]);
    NSScreen *screen = [native screen];
    LOKA_VERIFY(screen != nil);
    const NSRect frame = [native frame];
    LOKA_VERIFY(NSContainsRect([screen visibleFrame], frame));
    const NSRect content = [native contentRectForFrameRect:frame];
    LOKA_VERIFY(content.size.width == 257 && content.size.height == 163);

    // Deliberately mirror NativeContentFrame / VisibleTopForScreen in
    // MacWindow.mm, including the menu-bar fallback and integer truncation.
    const NSRect screenFrame = [screen frame];
    const NSRect visibleFrame = [screen visibleFrame];
    CGFloat top = visibleFrame.origin.y + visibleFrame.size.height;
    if (visibleFrame.size.height >= screenFrame.size.height - 1.0)
    {
      top = screenFrame.origin.y + screenFrame.size.height
            - [[NSStatusBar systemStatusBar] thickness];
    }
    const loka::core::Frame actual(static_cast<int>(content.origin.x),
                                   static_cast<int>(top - (content.origin.y + content.size.height)),
                                   static_cast<int>(content.size.width),
                                   static_cast<int>(content.size.height));
    LOKA_VERIFY(window.nativeFrame().get() == actual);
  }
}

void testMacWindowDeclaredFrameStaysInsideVisibleFrame()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [NSApplication sharedApplication];
  {
    NullPlatformContext context;
    NSScreen *screen = [NSScreen mainScreen];
    LOKA_VERIFY(screen != nil);
    const NSRect visible = [screen visibleFrame];
    WindowProps props;
    // FrameRectForContent treats y as a downward offset from the visible top
    // to the outer frame's top; this request crosses the right and bottom edges.
    props.frame(static_cast<int>(NSMaxX(visible)) - 50,
                static_cast<int>(NSMaxY(visible)) - 50, 257, 163);
    props.visible(true);
    // Like the admission sibling's initial seed, construction enters the
    // visibility path and makeKeyAndOrderFront: synchronously.
    MacWindow window(&context, props);
    verifyPlacedFrame(window);

    const loka::core::Frame first = window.nativeFrame().get();
    {
      loka::core::StateTrackerGuard guard(window.getTracker());
      // Still off-screen horizontally, but a different y makes a missing
      // frame observer distinguishable from preserving the first placement.
      window.frameState().set(loka::core::Frame(
          static_cast<int>(NSMaxX(visible)) + 100, 40, 257, 163));
    }
    verifyPlacedFrame(window);
    LOKA_VERIFY(window.nativeFrame().get() != first);
  }
  [pool drain];
}
