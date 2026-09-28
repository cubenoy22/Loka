#include "StackSpansTests.hpp"
#include "support/TestVerify.hpp"
#include "support/LokaAllocFailure.hpp"
#include "app/layout/StackSpans.hpp"
#include "app/nodes/nestable/ScrollView.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/controls/Button.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "platform/null/context/NullTextContext.hpp"
#include "app/nodes/AttributedText.hpp"

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace loka::app::layout;
  using namespace loka::core;

  void window(const StackSpans &spans, int top, int bottom, unsigned first, unsigned count)
  {
    const LazyWindow result = spans.indicesIn(top, bottom);
    LOKA_VERIFY(result.first == first);
    LOKA_VERIFY(result.count == count);
  }
} // namespace

void testStackSpansSelection()
{
  StackSpans spans;
  LOKA_VERIFY(!spans.valid());
  LOKA_VERIFY(spans.begin(5));
  LOKA_VERIFY(spans.append(0, 10));
  LOKA_VERIFY(spans.append(10, 10));
  LOKA_VERIFY(spans.append(10, 25));
  LOKA_VERIFY(spans.append(25, 40));
  LOKA_VERIFY(spans.append(40, 40));
  LOKA_VERIFY(!spans.valid());
  LOKA_VERIFY(spans.finish());
  LOKA_VERIFY(spans.total() == 40);
  window(spans, 0, 10, 0, 1);
  window(spans, 10, 25, 2, 1);
  window(spans, 9, 11, 0, 3);
  window(spans, 25, 40, 3, 1);
  window(spans, 39, 41, 3, 2);
  window(spans, 40, 50, 0, 0);
  window(spans, 12, 12, 0, 0);
  window(spans, -20, 0, 0, 0);
  spans.placed(Frame(0, 5, 80, 20));
  LOKA_VERIFY(spans.placedViewport().y == 5);
  spans.invalidate();
  LOKA_VERIFY(!spans.valid());
  LOKA_VERIFY(spans.placedViewport().height == 0);
  LOKA_VERIFY(spans.begin(0));
  LOKA_VERIFY(spans.finish());
  window(spans, 0, 20, 0, 0);
  LOKA_VERIFY(spans.begin(1));
  LOKA_VERIFY(!spans.append(0, SHRT_MAX + 1));
  LOKA_VERIFY(!spans.valid());
}

void testStackSpansAllocationRefusal()
{
  StackSpans spans;
  loka::core::testing::failLokaAllocRaw("TextLineBreaker", "Table", 1);
  const bool built = spans.begin(100);
  loka::core::testing::failLokaAllocRaw("TextLineBreaker", "Table", 0);
  LOKA_VERIFY(!built);
  LOKA_VERIFY(!spans.valid());
  LOKA_VERIFY(spans.begin(100));
  for (int i = 0; i < 100; ++i)
    LOKA_VERIFY(spans.append(i, i + 1));
  LOKA_VERIFY(spans.finish());
}

void testStackSpansColumnCollection()
{
  NullScenePlatformController platform;
  ScrollViewNode scroll((ScrollViewProps()));
  StackNode *column = new StackNode(StackProps(STACK_AXIS_COLUMN));
  scroll.addChild(column);
  for (int i = 0; i < 4; ++i)
    column->addChild(new TextNode(TextProps("text")));
  LayoutState state;
  state.x = 3;
  state.y = 7;
  state.width = 100;
  state.height = 80;
  state.spacing = 3;
  StackSpans spans;
  LOKA_VERIFY(platform.projectLayoutForTesting(&scroll, state, 0, &spans) == 87);
  LOKA_VERIFY(spans.valid());
  LOKA_VERIFY(spans.size() == 4);
  unsigned index = 0;
  for (Node *child = column->childrenHead(); child; child = child->nextInComposition, ++index)
  {
    TextNode *text = static_cast<TextNode *>(child);
    NullTextContext *context = static_cast<NullTextContext *>(child->getContext());
    LOKA_VERIFY(context != 0);
    LOKA_VERIFY(context->commitPresented(text->props.text_->get(), platform.paintScope()));
    const PaintQuery query = {platform.paintScope(), PLACEMENT_ELIGIBLE};
    const PaintAnswer placed = context->queryPaintDamage(query);
    LOKA_VERIFY(placed.kind == PAINT_ANSWER_EXACT);
    LOKA_VERIFY(spans.start(index) == placed.damage.y - state.y);
    LOKA_VERIFY(spans.end(index) == placed.damage.y - state.y + context->measurement().height() + state.spacing);
  }
  LOKA_VERIFY(spans.total() == 60);
  const unsigned long before = platform.leafLayoutVisitsForTesting();
  const LazyWindow range = {1, 2};
  platform.projectLayoutForTesting(&scroll, state, &range, &spans);
  LOKA_VERIFY(platform.leafLayoutVisitsForTesting() - before == 2);
  // A changed visited advance repairs all four leaves in this same call.
  state.spacing = 4;
  const unsigned long beforeRepair = platform.leafLayoutVisitsForTesting();
  platform.projectLayoutForTesting(&scroll, state, &range, &spans);
  LOKA_VERIFY(platform.leafLayoutVisitsForTesting() - beforeRepair == 5);
  LOKA_VERIFY(spans.valid() && spans.total() == 64);
  column->addChild(new TextNode(TextProps("new child")));
  const unsigned long beforeStructure = platform.leafLayoutVisitsForTesting();
  platform.projectLayoutForTesting(&scroll, state, &range, &spans);
  LOKA_VERIFY(platform.leafLayoutVisitsForTesting() - beforeStructure == 5);
  LOKA_VERIFY(spans.valid() && spans.size() == 5 && spans.total() == 80);
  // A refused viewport must not leave a table from a prior successful pass.
  LayoutState refused = state;
  refused.y = SHRT_MAX;
  platform.projectLayoutForTesting(&scroll, refused, &range, &spans);
  LOKA_VERIFY(!spans.valid());
  platform.projectLayoutForTesting(&scroll, state, 0, &spans);
  LOKA_VERIFY(spans.valid());
  Node *detached = scroll.detachChildren();
  platform.projectLayoutForTesting(&scroll, state, &range, &spans);
  LOKA_VERIFY(!spans.valid());
  scroll.addChild(detached);
  platform.projectLayoutForTesting(&scroll, state, 0, &spans);
  LOKA_VERIFY(spans.valid());
  column->addChild(new ButtonNode(Button("button").props));
  platform.projectLayoutForTesting(&scroll, state, &range, &spans);
  LOKA_VERIFY(!spans.valid());
}

void testStackSpansEligibility()
{
  NullScenePlatformController platform;
  StackNode column((StackProps(STACK_AXIS_COLUMN)));
  column.addChild(new TextNode(TextProps("text")));
  column.addChild(new AttributedTextNode(AttributedTextProps(Styled(String::Literal("styled"), TextStyle()))));
  LOKA_VERIFY(mayBandColumn(&column));
  column.addChild(new ButtonNode(Button("button").props));
  LOKA_VERIFY(!mayBandColumn(&column));
  StackNode row((StackProps(STACK_AXIS_ROW)));
  LOKA_VERIFY(!mayBandColumn(&row));
}

void testScrollViewOwnLayoutInputs()
{
  NullScenePlatformController platform;
  MutableState<int> storage(0);
  PushStateTracker tracker;
  tracker.addState(&storage);
  NodeState<int> offset(&storage, &tracker);
  StdCompositionNode owner((StdCompositionProps()));
  ScrollViewNode scroll((ScrollViewProps(offset)));
  scroll.setPropsTypeId(ScrollViewProps::staticTypeId());
  BoundaryNode::declareBoundaryDirtySources(&scroll, &owner);
  scroll.takeLayoutInputs();
  scroll.props.offset_.set(10);
  LOKA_VERIFY(scroll.takeLayoutInputs() == NODE_DIRTY_LAYOUT);
  LOKA_VERIFY(scroll.takeLayoutInputs() == NODE_DIRTY_NONE);
  ScrollViewDefinition next((ScrollViewProps(offset)));
  LOKA_VERIFY(next.applyPropsToNode(&scroll));
  LOKA_VERIFY(scroll.takeLayoutInputs() == (NODE_DIRTY_PROPS | NODE_DIRTY_LAYOUT));
  LOKA_VERIFY(scroll.takeLayoutInputs() == NODE_DIRTY_NONE);
  owner.clearObservedStateEntries();
}
