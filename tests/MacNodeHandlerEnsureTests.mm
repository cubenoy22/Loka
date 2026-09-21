#include "app/nodes/controls/TextEditor.hpp"
#include "MacNodeHandlerEnsureTests.hpp"
#include "support/TestVerify.hpp"
#include "support/RailTextLayoutFixture.hpp"
#include <cmath>

#include <AppKit/AppKit.h>
#include <cstdio>

#include "MacScenePlatformController.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/layout/FallbackControlMetrics.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/controls/ScrollBar.hpp"
#include "app/scene/projection/RetainedNodeHandler.hpp"

namespace
{
  void verifyFixedColumnFitsAtBothScales()
  {
    using namespace loka::app;
    const int count = 3;
    const int height = 40 + count * layout::FallbackControlMetrics::kButtonHeight
                       + (count - 1) * layout::FallbackControlMetrics::kVerticalSpacing;
    const RailMetrics metrics[] = {RailMetrics(), loka::macos::DefaultRailMetrics()};
    for (int scaleIndex = 0; scaleIndex < 2; ++scaleIndex)
    {
      const loka::macos::MacProjection allocation(0, metrics[scaleIndex]);
      NSView *root = [[NSView alloc] initWithFrame:allocation.projectClientSize(257, height).r];
      LOKA_VERIFY(root != nil);
      {
        MacScenePlatformController controller((void *)root, metrics[scaleIndex]);
        LOKA_VERIFY(controller.projection().clientCapacityToLu([root bounds].size.width) == 257);
        LOKA_VERIFY(controller.projection().clientCapacityToLu([root bounds].size.height) == height);
        if (scaleIndex == 1)
        {
          LOKA_VERIFY(controller.projection().projectEdge(8).pt == 10);
          LOKA_VERIFY(controller.projection().capacityToLu(301) == 240);
        }
        StackNode column((StackProps(STACK_AXIS_COLUMN)));
        for (int i = 0; i < count; ++i)
          column.addChild(new ButtonNode(ButtonProps()));
        controller.onChange(&column, loka::app::scene::NODE_DIRTY_NONE, false);
        controller.relayout(0, 0);
        NSArray *children = [root subviews];
        LOKA_VERIFY([children count] == static_cast<NSUInteger>(count));
        CGFloat bottom = 0;
        for (NSUInteger i = 0; i < [children count]; ++i)
        {
          const NSRect frame = [[children objectAtIndex:i] frame];
          if (NSMaxY(frame) > bottom)
            bottom = NSMaxY(frame);
        }
        LOKA_VERIFY(bottom == controller.projection().projectEdge(height - 20).pt);
        LOKA_VERIFY(bottom <= [root bounds].size.height);
        controller.onChange(0, loka::app::scene::NODE_DIRTY_NONE, false);
      }
      [root release];
    }
  }

  void verifyTextColumnExtents()
  {
    using namespace loka::app;
    const RailMetrics defaults = loka::macos::DefaultRailMetrics();
    // The middle leg isolates fontScale from spaceScale.
    const RailMetrics metrics[] = {RailMetrics(), RailMetrics(defaults.fontScale, Ratio()), defaults};
    const char *strings[] = {"First", "First\nSecond"};
    for (int scaleIndex = 0; scaleIndex < 3; ++scaleIndex)
      for (int boxed = 0; boxed < 2; ++boxed)
        for (int sample = 0; sample < 2; ++sample)
        {
          const loka::macos::MacProjection allocation(0, metrics[scaleIndex]);
          NSView *root = [[NSView alloc] initWithFrame:allocation.projectClientSize(340, 250).r];
          LOKA_VERIFY(root != nil);
          {
            MacScenePlatformController controller(root, metrics[scaleIndex]);
            RailTextLayoutFixture fixture(boxed != 0, strings[sample]);
            controller.onChange(&fixture.column, loka::app::scene::NODE_DIRTY_NONE, false);
            controller.relayout(0, 0);
            const loka::macos::MacProjection &projection = controller.projection();
            LOKA_VERIFY(projection.clientCapacityToLu([root bounds].size.height) == 250);
            NSArray *views = [root subviews];
            LOKA_VERIFY([views count] == static_cast<NSUInteger>(boxed ? 5 : 3));
            NSView *page = [views objectAtIndex:0];
            NSView *caption = [views objectAtIndex:1];
            NSButton *button = (NSButton *)[views objectAtIndex:boxed ? 3 : 2];
            LOKA_VERIFY([button isKindOfClass:[NSButton class]]);

            // Reference native measurement: pin both the inverse and the lu
            // padding, not an equality that independent font/space ratios lack.
            NSFont *font = (NSFont *)controller.textFont(FontSize<18>());
            NSTextFieldCell *cell = [[NSTextFieldCell alloc] initTextCell:@"First"];
            [cell setFont:font];
            [cell setWraps:YES];
            [cell setScrollable:NO];
            [cell setLineBreakMode:NSLineBreakByWordWrapping];
            const NSRect bounds = NSMakeRect(0, 0, projection.projectLength(0, 300).pt, 10000);
            const CGFloat oneLine = [cell cellSizeForBounds:bounds].height;
            [cell setStringValue:[NSString stringWithUTF8String:strings[sample]]];
            const CGFloat nativeHeight = [cell cellSizeForBounds:bounds].height;
            [cell release];
            // Short words cannot soft-wrap at 300 lu; the newline is the only
            // line break. NSCell's fixed padding can keep the ratio below two.
            const int lines = static_cast<int>(std::floor(nativeHeight / oneLine + 0.5));
            LOKA_VERIFY(lines == sample + 1);
            const int fontHeight = projection.measurementToLu(
                [font ascender] + std::fabs([font descender]) + [font leading]);
            const int minimum = fontHeight > 20 ? fontHeight : 20;
            const int measured = projection.measurementToLu(nativeHeight) + 2;
            const int textHeight = measured > minimum ? measured : minimum;
            const int captionY = boxed ? 190 : 20 + textHeight + 12;
            const int buttonY = captionY + 20 + 12;
            LOKA_VERIFY(NSEqualRects([page frame], projection.projectFrame(
                loka::core::Frame(20, 20, 300, textHeight)).r));
            LOKA_VERIFY([caption frame].origin.y == projection.projectEdge(captionY).pt);
            LOKA_VERIFY(NSMinY([button frame]) == projection.projectEdge(buttonY).pt);
            LOKA_VERIFY(NSMaxY([button frame]) == projection.projectEdge(buttonY + 32).pt);
            // The actual Scrapbook fixed Box prevents measured text changes
            // reaching the caption/buttons, for either one or two native lines.
            if (boxed)
              LOKA_VERIFY(buttonY + 32 == 254);
            std::printf("  Mac text extent: font=%d/%d space=%d/%d boxed=%d lines=%d "
                        "nativeText=%.2f textLu=%d buttonLu=%d..%d framePt=%.2f..%.2f clientPt=%.2f\n",
                        metrics[scaleIndex].fontScale.num, metrics[scaleIndex].fontScale.den,
                        metrics[scaleIndex].spaceScale.num, metrics[scaleIndex].spaceScale.den,
                        boxed, lines, static_cast<double>(nativeHeight), textHeight, buttonY, buttonY + 32,
                        static_cast<double>(NSMinY([button frame])), static_cast<double>(NSMaxY([button frame])),
                        static_cast<double>([root bounds].size.height));
            controller.onChange(0, loka::app::scene::NODE_DIRTY_NONE, false);
          }
          [root release];
        }
  }

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
      // Model the modal macOS dialog hazard from the ensure side: synchronous
      // app code REPLACES the node's live context (here with none) before
      // ensureContext returns, so the platform controller's local pointer is
      // stale on return. setContext(0) on a still-ATTACHED node is a live
      // replacement, not the retire door -- releaseContext's terminal
      // delivery assumes RETIRED was already written, so nothing is delivered
      // here. That is deliberate: this test owns the ensure re-read contract.
      // Retirement terminal delivery and queued native destruction are a
      // different contract, covered by the lifecycle-fact tests, and a
      // regression confined to those would not turn this test red.
      // Do not touch ctx after this call; it has been reclaimed.
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
  verifyFixedColumnFitsAtBothScales();
  verifyTextColumnExtents();
  NSView *root = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 320, 240)];
  LOKA_VERIFY(root != nil);
  {
    MacScenePlatformController controller((void *)root, loka::app::RailMetrics());

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

    // -- TextEditor: installs its multiline context even with unavailable props --
    loka::app::TextEditorNode editor((loka::app::TextEditorProps()));
    LOKA_VERIFY(controller.prepareProjectedLayout(&editor, state));
    LOKA_VERIFY(editor.getContext());
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
    LOKA_VERIFY(countChildViews(root) == childrenWithText + 1 &&
                "a refusal must not materialize a native view");

    // -- Re-entrancy: afterAttach replaces the just-published context --
    gReentrantCreates = 0;
    gReentrantAttachReads = 0;
    gReentrantAfterAttaches = 0;
    gReentrantDestroys = 0;
    MacReentrantEnsureHandler reentrantHandler;
    LOKA_VERIFY(controller.registerNodeHandler(&reentrantHandler));
    loka::app::ButtonNode reentrantButton(buttonProps);
    LOKA_VERIFY(!controller.prepareProjectedLayout(&reentrantButton, state) &&
                "ensure must re-read the node after re-entrant attach work replaces its context");
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

#include "app/nodes/AttributedText.hpp"
#include "platform/MacNativeGeometry.hpp"
#include "support/LifecycleFactTestAccess.hpp"
#include "support/LokaAllocFailure.hpp"
#include "platform/String.hpp"

namespace
{
  NSTextField *AttributedField(NSView *root)
  {
    LOKA_VERIFY([[root subviews] count] == 1);
    return (NSTextField *)[[root subviews] objectAtIndex:0];
  }

  void VerifyAttributedHeight(loka::app::AttributedTextNode &node, MacScenePlatformController &controller,
                              NSView *root, short width)
  {
    loka::app::scene::LayoutState state;
    state.x = 3;
    state.y = 5;
    state.width = width;
    node.layoutProjected(&controller, state);
    NSTextField *field = AttributedField(root);
    const loka::macos::MacProjection &projection = controller.projection();
    const NSSize rendered = [[field cell] cellSizeForBounds:loka::macos::MacMeasurementBounds(
        projection.projectLength(state.x, state.x + width))];
    LOKA_VERIFY(state.height == projection.measurementToLu(rendered.height));
    LOKA_VERIFY(NSEqualRects([field frame], projection.projectFrame(
        loka::core::Frame(3, 5, width, state.height)).r));
  }

  class MacRefusedAttributedString : public loka::platform::String
  {
  public:
    virtual bool appendUtf8(std::string &) const { return false; }
  };
}

void testMacAttributedTextWholeLineProjection()
{
  using namespace loka::app;
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [NSApplication sharedApplication];
  const RailMetrics metrics[] = {RailMetrics(), RailMetrics(Ratio(13, 12), Ratio(5, 4))};
  for (int scale = 0; scale < 2; ++scale)
  {
    NSView *root = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 320, 240)];
    {
      MacScenePlatformController controller((void *)root, metrics[scale]);
      LOKA_VERIFY(controller.textShaping() == WHOLE_LINE);
      const TextStyle small = FontSize<12>();
      const TextStyle large = FontSize<24>() + Italic;
      // A supplementary character makes UTF-8 byte offsets and UTF-16 ranges
      // disagree. Splitting the small run must not create a new styled range.
      const AttributedString value = Styled("A", small) + Styled("\xF0\x9F\x98\x80 ", small)
          + Styled("large italic words wrap here", large);
      AttributedTextProps props(value);
      props.blockStyle_ = BlockStyle().wrap(TEXT_WRAP_WORD);
      AttributedTextNode node(props);
      loka::app::scene::LayoutState state;
      state.width = 90;
      // Pre-fix red: gRefusedMacAttributedText rejects this ensure.
      LOKA_VERIFY(controller.prepareProjectedLayout(&node, state));
      LOKA_VERIFY(node.getContext() != 0);
      VerifyAttributedHeight(node, controller, root, 90);
      NSTextField *field = AttributedField(root);
      NSAttributedString *native = [field attributedStringValue];
      NSRange range;
      NSFont *first = [native attribute:NSFontAttributeName atIndex:0 effectiveRange:&range];
      LOKA_VERIFY(first == (NSFont *)controller.textFont(small));
      LOKA_VERIFY(range.location == 0 && range.length == 4);
      NSFont *second = [native attribute:NSFontAttributeName atIndex:4 effectiveRange:&range];
      LOKA_VERIFY(second == (NSFont *)controller.textFont(large));
      LOKA_VERIFY(range.location == 4 && range.length == [native length] - 4);
      const CGFloat narrowHeight = [field frame].size.height;
      VerifyAttributedHeight(node, controller, root, 260);
      LOKA_VERIFY([field frame].size.height < narrowHeight);
      loka::app::scene::NodeContext *context = node.getContext();
      node.props.blockStyle_ = BlockStyle().wrap(TEXT_WRAP_CHAR);
      context->onPropsApplied();
      VerifyAttributedHeight(node, controller, root, 90);
      LOKA_VERIFY([[field cell] lineBreakMode] == NSLineBreakByCharWrapping);
      NSParagraphStyle *paragraph = [[field attributedStringValue]
          attribute:NSParagraphStyleAttributeName atIndex:0 effectiveRange:0];
      LOKA_VERIFY([paragraph lineBreakMode] == NSLineBreakByCharWrapping);
      node.props.blockStyle_ = BlockStyle().wrap(TEXT_WRAP_NONE).truncation(TEXT_TRUNCATION_ELLIPSIS);
      context->onPropsApplied();
      VerifyAttributedHeight(node, controller, root, 90);
      LOKA_VERIFY([[field cell] lineBreakMode] == NSLineBreakByTruncatingTail);
      LOKA_VERIFY(![[field cell] wraps]);
      LOKA_VERIFY(node.getContext() == context);
    }
    LOKA_VERIFY([[root subviews] count] == 0);
    [root release];
  }
  [pool drain];
}

void testMacAttributedTextRetainedLifecycle()
{
  using namespace loka::app;
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [NSApplication sharedApplication];
  NSView *root = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 320, 240)];
  {
    MacScenePlatformController controller((void *)root, RailMetrics());
    AttributedTextProps props(Styled("before", FontSize<12>()));
    props.blockStyle_ = BlockStyle().wrap(TEXT_WRAP_WORD);
    AttributedTextNode node(props);
    // A node placed directly (not through the definition factory) carries no
    // props type id; applyPropsToNode's compatibility check needs it.
    node.setPropsTypeId(AttributedTextProps::staticTypeId());
    VerifyAttributedHeight(node, controller, root, 100);
    loka::app::scene::NodeContext *context = node.getContext();
    NSTextField *field = AttributedField(root);
    NSAttributedString *before = [[field attributedStringValue] retain];
    loka::app::scene::NotifySubtreeNodeDetached(&node);
    loka::app::scene::LifecycleFactTestAccess::DeliverFacts(&node);
    LOKA_VERIFY([field isHidden]);
    LOKA_VERIFY([[field attributedStringValue] isEqualToAttributedString:before]);
    loka::app::scene::NotifySubtreeNodeAttached(&node);
    loka::app::scene::LifecycleFactTestAccess::DeliverFacts(&node);
    LOKA_VERIFY(![field isHidden]);
    LOKA_VERIFY(node.getContext() == context);
    LOKA_VERIFY([[field attributedStringValue] isEqualToAttributedString:before]);
    [before release];
    loka::app::scene::NotifySubtreeNodeDetached(&node);
    loka::app::scene::LifecycleFactTestAccess::DeliverFacts(&node);
    AttributedTextProps replacement(Styled("updated while hidden", FontSize<24>() + Italic));
    replacement.blockStyle_ = BlockStyle().wrap(TEXT_WRAP_WORD);
    AttributedTextDefinition definition(replacement);
    LOKA_VERIFY(definition.applyPropsToNode(&node));
    VerifyAttributedHeight(node, controller, root, 100);
    LOKA_VERIFY([field isHidden]);
    loka::app::scene::NotifySubtreeNodeAttached(&node);
    loka::app::scene::LifecycleFactTestAccess::DeliverFacts(&node);
    LOKA_VERIFY(![field isHidden]);
    LOKA_VERIFY([[field stringValue] isEqualToString:@"updated while hidden"]);
    LOKA_VERIFY(node.getContext() == context);
    loka::app::scene::LifecycleFactTestAccess::MarkSubtreeRetired(&node);
    loka::app::scene::LifecycleFactTestAccess::DeliverFacts(&node);
    LOKA_VERIFY([[root subviews] count] == 0);
    LOKA_VERIFY([[field stringValue] length] == 0);
  }
  [root release];
  [pool drain];
}

void testMacAttributedTextRefusalClearsProjection()
{
  using namespace loka::app;
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [NSApplication sharedApplication];
  NSView *root = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 320, 240)];
  {
    MacScenePlatformController controller((void *)root, RailMetrics());
    AttributedTextProps props(Styled("visible", FontSize<12>()));
    AttributedTextNode node(props);
    VerifyAttributedHeight(node, controller, root, 100);
    NSTextField *field = AttributedField(root);
    const loka::core::String text("allocation refusal");
    loka::core::testing::failLokaAllocRaw("AttributedString", "Segments", 1);
    const AttributedString refused = Styled(text, Italic);
    loka::core::testing::allowLokaAllocRaw();
    LOKA_VERIFY(!refused.valid());
    node.props.text(refused);
    loka::app::scene::LayoutState state;
    state.width = 100;
    node.layoutProjected(&controller, state);
    LOKA_VERIFY(state.height == 0 && [[field stringValue] length] == 0);
    node.props.text(Styled("recovered", Italic));
    VerifyAttributedHeight(node, controller, root, 100);
    const loka::core::String unreadable(loka::core::Managed<loka::platform::String>::Wrap(new MacRefusedAttributedString()));
    node.props.text(Styled(unreadable, Italic));
    node.layoutProjected(&controller, state);
    LOKA_VERIFY(state.height == 0 && [[field stringValue] length] == 0);
    node.props.text(Styled("recovered again", Italic));
    VerifyAttributedHeight(node, controller, root, 100);
  }
  [root release];
  [pool drain];
}
