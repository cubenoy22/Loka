#include "MacNodeHandlerEnsureTests.hpp"
#include "support/TestVerify.hpp"

#include <AppKit/AppKit.h>
#include <cstdio>

#include "MacScenePlatformController.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/ScrollBar.hpp"
#include "app/scene/projection/RetainedNodeHandler.hpp"

namespace
{
  int gReentrantCreates = 0;
  int gReentrantAttachReads = 0;
  int gReentrantAfterAttaches = 0;
  int gReentrantDestroys = 0;

  class MacReentrantEnsureContext : public loka::app::scene::NodeContext
  {
  public:
    MacReentrantEnsureContext()
        : loka::app::scene::NodeContext()
    {
      ++gReentrantCreates;
    }

    virtual ~MacReentrantEnsureContext()
    {
      ++gReentrantDestroys;
    }

    void readLifecycleFactOnAttach()
    {
      ++gReentrantAttachReads;
    }
  };

  class MacReentrantEnsureHandler
      : public loka::app::scene::RetainedNodeHandler<MacReentrantEnsureHandler,
                                                     loka::app::ButtonNode,
                                                     MacReentrantEnsureContext>
  {
  public:
    static loka::app::ButtonNode *cast(loka::app::scene::Node *node)
    {
      return node ? node->asButtonNode() : 0;
    }

    static MacReentrantEnsureContext *create(loka::app::ButtonNode *,
                                             loka::app::scene::IPlatformController *,
                                             const loka::app::scene::LayoutState &)
    {
      return new MacReentrantEnsureContext();
    }

    static void afterAttach(MacReentrantEnsureContext *ctx)
    {
      ++gReentrantAfterAttaches;
      loka::app::scene::Node *owner = ctx->owner();
      LOKA_VERIFY(owner != 0);
      // Model the modal macOS dialog hazard: synchronous app code recomposes
      // and retires the context before ensureContext returns to the platform
      // controller. Do not touch ctx after this call; it has been reclaimed.
      owner->setContext(0);
    }
  };

  NSUInteger countChildViews(NSView *root)
  {
    return [[root subviews] count];
  }

  bool frameEquals(NSView *view, CGFloat x, CGFloat y, CGFloat width, CGFloat height)
  {
    const NSRect frame = [view frame];
    return frame.origin.x == x && frame.origin.y == y && frame.size.width == width && frame.size.height == height;
  }
} // namespace

// macOS twin of testWin32NodeHandlerEnsureContract. Besides pinning context
// publication, reuse, relayout, and typed refusal through the native controller
// seam, the final leg discriminates PR #254's modal re-entrancy hazard: the
// handler must return the Node's context after afterAttach, not its stale local.
void testMacNodeHandlerEnsureContract()
{
  std::printf("\n==== [testMacNodeHandlerEnsureContract] start ====\n");
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [NSApplication sharedApplication];
  NSView *root = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 320, 240)];
  LOKA_VERIFY(root != nil);
  {
    MacScenePlatformController controller((void *)root);

    // -- Button: full contract through the root view's child census --
    loka::app::ButtonProps buttonProps;
    loka::app::ButtonNode button(buttonProps);

    loka::app::scene::LayoutState state;
    state.x = 10;
    state.y = 20;
    state.width = 100;
    state.height = 30;
    const NSUInteger childrenBeforeEnsure = countChildViews(root);
    LOKA_VERIFY(controller.prepareProjectedLayout(&button, state));

    loka::app::scene::NodeContext *ctx = button.getContext();
    LOKA_VERIFY(ctx != 0 && "ensure must publish the created context through setContext");
    LOKA_VERIFY(countChildViews(root) == childrenBeforeEnsure + 1);
    NSView *buttonView = [[root subviews] objectAtIndex:childrenBeforeEnsure];
    LOKA_VERIFY(frameEquals(buttonView, 10, 20, 100, 30));

    // Second ensure with new geometry: same context, same view population,
    // view moved by the relayout path -- not recreated.
    state.x = 40;
    state.y = 50;
    state.width = 120;
    state.height = 40;
    LOKA_VERIFY(controller.prepareProjectedLayout(&button, state));
    LOKA_VERIFY(button.getContext() == ctx && "re-ensure must reuse the existing context, not recreate it");
    LOKA_VERIFY(countChildViews(root) == childrenBeforeEnsure + 1 &&
                "re-ensure must not materialize another native view");
    LOKA_VERIFY(frameEquals(buttonView, 40, 50, 120, 40) &&
                "re-ensure must route through relayout so the view follows the requested geometry");

    // -- Text: same contract on a second native node kind --
    loka::app::TextProps textProps;
    loka::app::TextNode text(textProps);
    state.x = 5;
    state.y = 100;
    state.width = 200;
    state.height = 16;
    LOKA_VERIFY(controller.prepareProjectedLayout(&text, state));
    loka::app::scene::NodeContext *textCtx = text.getContext();
    LOKA_VERIFY(textCtx != 0 && "ensure must publish the created context through setContext");
    const NSUInteger childrenWithText = countChildViews(root);
    LOKA_VERIFY(childrenWithText == childrenBeforeEnsure + 2);
    LOKA_VERIFY(controller.prepareProjectedLayout(&text, state));
    LOKA_VERIFY(text.getContext() == textCtx);
    LOKA_VERIFY(countChildViews(root) == childrenWithText);

    // -- ScrollBar: known unsupported kinds take the typed-refusal path --
    loka::app::ScrollBarProps scrollProps;
    loka::app::ScrollBarNode scrollBar(scrollProps);
    state.x = 5;
    state.y = 130;
    state.width = 120;
    state.height = 16;
    LOKA_VERIFY(!controller.prepareProjectedLayout(&scrollBar, state) &&
                "an unsupported kind must refuse, not project");
    LOKA_VERIFY(!scrollBar.getContext());
    LOKA_VERIFY(countChildViews(root) == childrenWithText &&
                "a refusal must not materialize a native view");

    // -- Re-entrancy: afterAttach retires the just-published context --
    gReentrantCreates = 0;
    gReentrantAttachReads = 0;
    gReentrantAfterAttaches = 0;
    gReentrantDestroys = 0;
    MacReentrantEnsureHandler reentrantHandler;
    LOKA_VERIFY(controller.registerNodeHandler(&reentrantHandler));
    loka::app::ButtonNode reentrantButton(buttonProps);
    LOKA_VERIFY(!controller.prepareProjectedLayout(&reentrantButton, state) &&
                "ensure must re-read the node after re-entrant attach work retires its context");
    LOKA_VERIFY(!reentrantButton.getContext());
    LOKA_VERIFY(gReentrantCreates == 1);
    LOKA_VERIFY(gReentrantAttachReads == 1);
    LOKA_VERIFY(gReentrantAfterAttaches == 1);
    LOKA_VERIFY(gReentrantDestroys == 1);

    std::printf("  button and text contexts reused; scrollbar refused; re-entrant ensure returned null\n");
    // Nodes leave scope before the controller. Their terminal facts remove
    // native views synchronously; the controller then drains retained objects.
  }
  LOKA_VERIFY(countChildViews(root) == 0);
  [root release];
  [pool drain];
  std::printf("==== [testMacNodeHandlerEnsureContract] PASSED ====\n");
}
