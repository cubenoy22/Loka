#include "MacNodeHandlerEnsureTests.hpp"
#include "MacObjCCompat.hpp"
#include "MacScenePlatformController.hpp"
#include "app/layout/FallbackControlMetrics.hpp"
#include "app/nodes/controls/Ribbon.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "support/TestVerify.hpp"
#include <cmath>
#include <string>

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace loka::dsl;
  using namespace loka::dsl::testing;

  CGFloat referenceCellWidth(NSString *title)
  {
    NSButtonCell *cell = [[NSButtonCell alloc] initTextCell:title];
    LOKA_VERIFY(cell != nil);
    [cell setBezelStyle:LOKA_MAC_BUTTON_BEZEL_STYLE];
    [cell setButtonType:LOKA_MAC_BUTTON_TYPE_MOMENTARY_PUSH_IN];
    const CGFloat width = [cell cellSize].width;
    [cell release];
    return width;
  }

  NSButton *buttonAt(NSView *root, NSUInteger index)
  {
    NSArray *views = [root subviews];
    LOKA_VERIFY(index < [views count]);
    NSButton *button = (NSButton *)[views objectAtIndex:index];
    LOKA_VERIFY([button isKindOfClass:[NSButton class]]);
    return button;
  }

  // A different type key bypasses the registered Stack handler while retaining
  // the Stack shape; this pins the controller's direct Row fallback separately.
  class DirectRow : public StackNode
  {
  public:
    DirectRow() : StackNode(StackProps(STACK_AXIS_ROW))
    {
      this->props.rowUndeclaredWidth_ = ROW_UNDECLARED_WIDTH_NATURAL;
    }
    virtual const void *nodeTypeKey() const { return NodeTypeToken<DirectRow>(); }
  };

  void verifyRibbonFrames(const RailMetrics &metrics)
  {
    NSView *root = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 800, 240)];
    LOKA_VERIFY(root != nil);
    {
      MacScenePlatformController controller(root, metrics);
      NodeDefinitionBase *definition =
          (RibbonControl() << RibbonItem("New") << RibbonItem("Save As...")
                           << RibbonItem("Fixed").width(120)).clone();
      LOKA_VERIFY(definition != 0);
      Scene scene(definition);
      LOKA_VERIFY(scene.mount(&controller));
      SceneTestAccess::updateAttached(scene, true);
      controller.relayout(640, 160);
      LOKA_VERIFY([[root subviews] count] == 3);
      const loka::macos::MacProjection &projection = controller.projection();
      int x = 20;
      for (NSUInteger i = 0; i < 3; ++i)
      {
        NSButton *button = buttonAt(root, i);
        NSString *title = i == 0 ? @"New" : i == 1 ? @"Save As..." : @"Fixed";
        LOKA_VERIFY([[button title] isEqualToString:title]);
        const CGFloat points = referenceCellWidth(title);
        // The reference cell must match what the rail actually builds,
        // including AppKit's default font and all implicit cell settings.
        LOKA_VERIFY([[button cell] cellSize].width == points);
        const int width = i == 2 ? 120 : projection.measurementToLu(points);
        const NSRect frame = [button frame];
        LOKA_VERIFY(frame.origin.x == projection.projectEdge(x).pt);
        LOKA_VERIFY(frame.size.width == projection.projectLength(x, x + width).pt);
        if (metrics.spaceScale.isUnit())
          LOKA_VERIFY(frame.size.width == (i == 2 ? 120 : std::ceil(points)));
        if (i)
          LOKA_VERIFY(frame.origin.x > NSMaxX([buttonAt(root, i - 1) frame]));
        x += width + layout::FallbackControlMetrics::kHorizontalSpacing;
      }
      SceneTestAccess::unmount(scene);
    }
    LOKA_VERIFY([[root subviews] count] == 0);
    [root release];
  }
}

void testMacButtonNaturalWidthMatchesCell()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [NSApplication sharedApplication];
  NSView *root = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 640, 240)];
  LOKA_VERIFY(root != nil);
  {
    MacScenePlatformController controller(root, RailMetrics());
    ButtonNode newButton(ButtonProps().text(loka::core::String::Literal("New")));
    ButtonNode saveButton(ButtonProps().text(loka::core::String::Literal("Save As...")));
    short newWidth = 0, saveWidth = 0, repeated = 0;
    LOKA_VERIFY(controller.queryNaturalWidth(&newButton, newWidth));
    LOKA_VERIFY(controller.queryNaturalWidth(&saveButton, saveWidth));
    LOKA_VERIFY(newWidth < saveWidth);
    LOKA_VERIFY(newWidth == std::ceil(referenceCellWidth(@"New")));
    LOKA_VERIFY(saveWidth == std::ceil(referenceCellWidth(@"Save As...")));
    LOKA_VERIFY(controller.queryNaturalWidth(&newButton, repeated));
    LOKA_VERIFY(repeated == newWidth);
    StackNode other((StackProps()));
    repeated = 17;
    LOKA_VERIFY(!controller.queryNaturalWidth(&other, repeated));
    LOKA_VERIFY(!controller.queryNaturalWidth(0, repeated));
    LOKA_VERIFY(repeated == 17);
    ButtonNode oversized(ButtonProps().text(loka::core::String(std::string(32768, 'W'))));
    LOKA_VERIFY(!controller.queryNaturalWidth(&oversized, repeated));
    LOKA_VERIFY(repeated == 17);

    ButtonNode untitled((ButtonProps()));
    controller.onChange(&untitled, NODE_DIRTY_NONE, false);
    controller.relayout(640, 160);
    NSButton *native = buttonAt(root, 0);
    LOKA_VERIFY([[native title] isEqualToString:@"Button"]);
    LOKA_VERIFY(controller.queryNaturalWidth(&untitled, repeated));
    LOKA_VERIFY(repeated == std::ceil([[native cell] cellSize].width));
    controller.onChange(0, NODE_DIRTY_NONE, false);
  }
  [root release];
  [pool drain];
}

void testMacRibbonNaturalWidthFrames()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [NSApplication sharedApplication];
  verifyRibbonFrames(RailMetrics());
  verifyRibbonFrames(loka::macos::DefaultRailMetrics());
  [pool drain];
}

void testMacDirectRowNaturalWidthFrames()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [NSApplication sharedApplication];
  NSView *root = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 640, 240)];
  LOKA_VERIFY(root != nil);
  {
    MacScenePlatformController controller(root, RailMetrics());
    DirectRow row;
    row.addChild(new ButtonNode(ButtonProps().text(loka::core::String::Literal("New"))));
    row.addChild(new ButtonNode(ButtonProps().text(loka::core::String::Literal("Save As..."))));
    controller.onChange(&row, NODE_DIRTY_NONE, false);
    controller.relayout(640, 160);
    LOKA_VERIFY([[root subviews] count] == 2);
    NSButton *first = buttonAt(root, 0);
    NSButton *second = buttonAt(root, 1);
    LOKA_VERIFY([first frame].size.width == std::ceil(referenceCellWidth(@"New")));
    LOKA_VERIFY([second frame].size.width == std::ceil(referenceCellWidth(@"Save As...")));
    LOKA_VERIFY([second frame].origin.x == NSMaxX([first frame])
                + layout::FallbackControlMetrics::kHorizontalSpacing);
    controller.onChange(0, NODE_DIRTY_NONE, false);
  }
  LOKA_VERIFY([[root subviews] count] == 0);
  [root release];
  [pool drain];
}

void testMacOrdinaryRowStillSharesWidth()
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [NSApplication sharedApplication];
  NSView *root = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 640, 240)];
  LOKA_VERIFY(root != nil);
  {
    MacScenePlatformController controller(root, RailMetrics());
    NodeDefinitionBase *definition = (Row() << Button("New") << Button("Save As...")).clone();
    LOKA_VERIFY(definition != 0);
    Scene scene(definition);
    LOKA_VERIFY(scene.mount(&controller));
    SceneTestAccess::updateAttached(scene, true);
    controller.relayout(640, 160);
    LOKA_VERIFY([[root subviews] count] == 2);
    const NSRect first = [buttonAt(root, 0) frame];
    const NSRect second = [buttonAt(root, 1) frame];
    const int gap = layout::FallbackControlMetrics::kHorizontalSpacing;
    LOKA_VERIFY(first.size.width == (640 - 40 - gap) / 2);
    LOKA_VERIFY(second.size.width == first.size.width);
    LOKA_VERIFY(second.origin.x == NSMaxX(first) + gap);
    SceneTestAccess::unmount(scene);
  }
  LOKA_VERIFY([[root subviews] count] == 0);
  [root release];
  [pool drain];
}
