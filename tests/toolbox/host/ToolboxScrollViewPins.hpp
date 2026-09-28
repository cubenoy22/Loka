#ifndef LOKA_TOOLBOX_SCROLL_VIEW_PINS_HPP
#define LOKA_TOOLBOX_SCROLL_VIEW_PINS_HPP
#include "app/nodes/nestable/RowColumn.hpp"
#include "core/StateTracker.hpp"

namespace
{
  struct ScrollFixture
  {
    ToolboxWindow window;
    ToolboxScenePlatformController controller;
    loka::core::MutableState<int> offset;
    loka::core::PushStateTracker tracker;
    NodeState<int> fact;
    ScrollViewNode scroll;
    StackNode *column;
    explicit ScrollFixture(unsigned count = 20, bool button = false)
        : controller(&window), offset(0), fact(&offset, &tracker),
          scroll(ScrollViewProps().offset(fact)), column(new StackNode(StackProps(STACK_AXIS_COLUMN)))
    {
      tracker.addState(&offset);
      LOKA_VERIFY(RegisterToolboxBuiltInSupport(controller));
      scroll.setPropsTypeId(ScrollViewProps::staticTypeId());
      scroll.addChild(column);
      for (unsigned i = 0; i < count; ++i)
      {
        Node *node = button && i == count - 1 ? static_cast<Node *>(new ButtonNode(ButtonProps()))
            : new AttributedTextNode(AttributedText(Styled("abcd", FontSize<12>())).props);
        node->setPropsTypeId(AttributedTextProps::staticTypeId());
        column->addChild(node);
      }
    }
    ~ScrollFixture()
    {
      LifecycleFactTestAccess::MarkSubtreeRetired(&scroll);
      LifecycleFactTestAccess::DeliverFacts(&scroll);
      controller.releaseNodeContexts(&scroll);
      LOKA_VERIFY(!controller.scrollSpans());
      delete scroll.detachChildren();
      controller.retired.clear();
    }
    unsigned step(int value, short width = 100, short height = 34, short y = 0)
    {
      fact.set(value);
      scroll.requeueLayoutInputs(NODE_DIRTY_LAYOUT);
      LayoutState seat = Seat(width);
      seat.x = 0; seat.y = y; seat.height = height;
      const unsigned before = controller.leafLayouts;
      controller.layoutScrollView(&scroll, seat, 0);
      return controller.leafLayouts - before;
    }
    AttributedTextNode *leaf(unsigned index)
    {
      Node *node = column->childrenHead();
      while (index--) node = node->nextInComposition;
      return static_cast<AttributedTextNode *>(node);
    }
    Rect paint(unsigned index)
    {
      return ToolboxAttributedTextContextAccess::paintRect(
          *static_cast<ToolboxAttributedTextContext *>(leaf(index)->getContext()));
    }
    Rect rect(unsigned index)
    {
      return ToolboxAttributedTextContextAccess::rect(
          *static_cast<ToolboxAttributedTextContext *>(leaf(index)->getContext()));
    }
  };

  void ScrollViewPins()
  {
    Pin("ToolboxScrollViewBandExitsAndEntries");
    {
      ScrollFixture f;
      LOKA_VERIFY(f.step(0) == 20);
      LOKA_VERIFY(f.controller.scrollSpans() && f.controller.scrollSpans()->valid());
      LOKA_VERIFY(f.controller.scrollMaximum == 306);
      LOKA_VERIFY(f.step(17) == 3);
      const Rect exited = f.paint(0);
      LOKA_VERIFY(EmptyRect(&exited));
      LOKA_VERIFY(f.rect(2).top == 17);
      for (unsigned i = 3; i < 20; ++i) { const Rect r = f.paint(i); LOKA_VERIFY(EmptyRect(&r)); }
      LOKA_VERIFY(f.step(255) == 16); // Bounding union includes the seek gap.
      LOKA_VERIFY(f.rect(15).top == 0);
      for (unsigned i = 0; i < 15; ++i) { const Rect r = f.paint(i); LOKA_VERIFY(EmptyRect(&r)); }
      LOKA_VERIFY(f.step(272) == 3); // Nonzero seek, not a full layout visit.
      LOKA_VERIFY(f.scroll.takeLayoutInputs() == NODE_DIRTY_NONE);
    }
    Pin("ToolboxScrollViewButtonFullPass");
    {
      ScrollFixture f(20, true);
      LOKA_VERIFY(f.step(0) == 20);
      LOKA_VERIFY(f.step(17) == 20);
      LOKA_VERIFY(!f.controller.scrollSpans());
    }
    Pin("ToolboxScrollViewEligibilityLoss");
    {
      ScrollFixture f;
      LOKA_VERIFY(f.step(0) == 20);
      f.column->addChild(new ButtonNode(ButtonProps()));
      LOKA_VERIFY(f.step(17) == 21);
      LOKA_VERIFY(!f.controller.scrollSpans()->valid());
    }
    Pin("ToolboxScrollViewStaleLeafSamePassRepair");
    {
      ScrollFixture f;
      LOKA_VERIFY(f.step(0) == 20);
      LOKA_VERIFY(AttributedText(Styled("abcd", FontSize<24>())).applyPropsToNode(f.leaf(4)));
      LOKA_VERIFY(f.step(17) == 3);
      LOKA_VERIFY(f.controller.scrollSpans()->valid());
      LOKA_VERIFY(f.controller.scrollMaximum == 306);
      LOKA_VERIFY(f.step(51) == 24); // Four detection visits, then all twenty.
      LOKA_VERIFY(f.controller.scrollMaximum == 318);
      LOKA_VERIFY(f.rect(5).top == 46);
      LOKA_VERIFY(f.controller.scrollSpans()->total() == 352);
    }
    Pin("ToolboxScrollViewFontPropsRepair");
    {
      ScrollFixture f;
      LOKA_VERIFY(f.step(0) == 20);
      for (unsigned i = 0; i < 20; ++i)
        LOKA_VERIFY(AttributedText(Styled("abcd", FontSize<24>())).applyPropsToNode(f.leaf(i)));
      LOKA_VERIFY(f.step(17) == 21);
      LOKA_VERIFY(f.controller.scrollSpans()->total() == 580);
      LOKA_VERIFY(f.controller.scrollMaximum == 546);
    }
    Pin("ToolboxScrollViewInvalidationAndReattach");
    {
      ScrollFixture f;
      LOKA_VERIFY(f.step(0) == 20);
      LOKA_VERIFY(f.step(17, 101) == 20);
      LOKA_VERIFY(f.step(34, 101) == 3);
      LOKA_VERIFY(ScrollView().offset(f.fact).applyPropsToNode(&f.scroll));
      LOKA_VERIFY(f.step(51, 101) == 20);
      f.controller.requestStructurePresent();
      LOKA_VERIFY(!f.controller.scrollSpans()->valid());
      LOKA_VERIFY(f.step(68, 101) == 20);
      NotifySubtreeNodeDetached(&f.scroll);
      LifecycleFactTestAccess::DeliverFacts(&f.scroll);
      NotifySubtreeNodeAttached(&f.scroll);
      LifecycleFactTestAccess::DeliverFacts(&f.scroll);
      LOKA_VERIFY(f.step(85, 101) == 20);
      LOKA_VERIFY(f.controller.scrollSpans()->valid());
      LOKA_VERIFY(f.step(85, 101, 34, SHRT_MAX) == 0);
      LOKA_VERIFY(!f.controller.scrollSpans()->valid());
      LOKA_VERIFY(f.scroll.takeLayoutInputs() == NODE_DIRTY_LAYOUT);
      LOKA_VERIFY(f.step(85, 101) == 20);
      DisposeRgn(f.controller.scrollViewClipRgn_);
      f.controller.scrollViewClipRgn_ = 0;
      LOKA_VERIFY(f.step(102, 101) == 0);
      LOKA_VERIFY(!f.controller.scrollSpans()->valid());
      LOKA_VERIFY(f.scroll.takeLayoutInputs() == NODE_DIRTY_LAYOUT);
      f.controller.scrollViewClipRgn_ = NewRgn();
    }
    Pin("ToolboxScrollViewBoundaryStructureBeforeLayoutReturn");
    {
      ScrollFixture f;
      MeasurementBoundary boundary;
      LOKA_VERIFY(f.step(0) == 20);
      for (unsigned route = 0; route < 2; ++route)
      {
        BoundaryLocalApplyInfo info;
        info.hasLayoutWork = true;
        info.hasStructureWork = route == 0;
        PlatformApplyPlan plan;
        plan.layoutChanged = true;
        plan.structureChanged = route != 0;
        plan.setPrimaryRoot(&boundary);
        f.controller.onBoundaryApply(&f.scroll, &boundary, info, plan);
        LOKA_VERIFY(!f.controller.scrollSpans()->valid());
        LOKA_VERIFY(f.step(17 + 17 * route) == 20);
      }
      // Same-count off-screen replacement crosses the real release/present door.
      AttributedTextNode *old = f.leaf(18);
      Node *replacement = new AttributedTextNode(AttributedText(Styled("new", FontSize<24>())).props);
      LifecycleFactTestAccess::MarkSubtreeRetired(old);
      f.controller.releaseNodeContexts(old);
      LOKA_VERIFY(f.column->replaceChild(old, replacement));
      delete old;
      LOKA_VERIFY(!f.controller.scrollSpans()->valid());
      LOKA_VERIFY(f.step(51) == 20);
      // Context creation may revoke an in-progress derived capture; the next
      // full pass collects once every context is installed.
      LOKA_VERIFY(f.step(68) == 20);
      LOKA_VERIFY(f.controller.scrollSpans()->valid());
      LOKA_VERIFY(f.controller.scrollSpans()->total() == 352);
    }
    Pin("ToolboxScrollViewLeafMeasurementRefusal");
    loka::core::testing::failLokaAllocRaw("ToolboxAttributedText", "Break", 0);
    {
      ScrollFixture f;
      LOKA_VERIFY(f.step(0) == 20);
      LOKA_VERIFY(AttributedText(Styled("changed", FontSize<24>())).applyPropsToNode(f.leaf(1)));
      loka::core::testing::failLokaAllocRaw("ToolboxAttributedText", "Break", 1);
      f.step(17);
      LOKA_VERIFY(!f.controller.scrollSpans()->valid());
      LOKA_VERIFY(f.scroll.takeLayoutInputs() == NODE_DIRTY_LAYOUT);
      LOKA_VERIFY(f.step(17) == 20);
      LOKA_VERIFY(f.controller.scrollSpans()->valid());
    }
    loka::core::testing::allowLokaAllocRaw();
    Pin("ToolboxScrollViewAllocationRefusalFullPlacement");
    const char *owners[] = {"ToolboxScrollView", "Managed", "TextLineBreaker"};
    const char *purposes[] = {"StackSpans", "ControlBlock", "Table"};
    for (unsigned failure = 0; failure < 3; ++failure)
    {
      loka::core::testing::failLokaAllocRaw(owners[failure], purposes[failure], 0);
      {
        ScrollFixture f(100);
        // Warm leaf tables so the injected table failure selects StackSpans only.
        LOKA_VERIFY(f.step(0) == 100);
        f.controller.destroyViewportScrollBarControl(&f.scroll, f.scroll.nativeLifetimeHint());
        loka::core::testing::failLokaAllocRaw(owners[failure], purposes[failure], 1);
        LOKA_VERIFY(f.step(17) == 100);
        loka::core::testing::failLokaAllocRaw(owners[failure], purposes[failure], 0);
        LOKA_VERIFY(!f.controller.scrollSpans() || !f.controller.scrollSpans()->valid());
        LOKA_VERIFY(f.controller.scrollMaximum == 1666);
        LOKA_VERIFY(f.rect(2).top == 17);
        const Rect exited = f.paint(0);
        LOKA_VERIFY(EmptyRect(&exited));
        LOKA_VERIFY(f.step(34) == 100);
        LOKA_VERIFY(f.controller.scrollSpans()->valid());
        LOKA_VERIFY(f.step(51) == 3);
      }
      loka::core::testing::allowLokaAllocRaw();
    }
    Pin("ToolboxScrollViewLeafContextRefusalRejectsCapture");
    {
      // A leaf whose context allocation is refused during the full capture must
      // not leave a zero-height span in a valid table (bot P2 on #1016): the
      // refused leaf leaves the pass refused, no table is retained, and the
      // next pass captures it in full. Characterization of the existing
      // refusal route; no production change was needed.
      loka::core::testing::failLokaAllocRaw("ToolboxAttributedText", "Context", 0);
      ScrollFixture f(100);
      loka::core::testing::failLokaAllocRaw("ToolboxAttributedText", "Context", 1);
      f.step(0);
      loka::core::testing::failLokaAllocRaw("ToolboxAttributedText", "Context", 0);
      LOKA_VERIFY(!f.leaf(0)->getContext());
      LOKA_VERIFY(!f.controller.scrollSpans() || !f.controller.scrollSpans()->valid());
      LOKA_VERIFY(f.step(17) == 100);
      LOKA_VERIFY(f.controller.scrollSpans()->valid());
      LOKA_VERIFY(f.controller.scrollSpans()->total() == f.controller.scrollSpans()->end(99));
    }
    loka::core::testing::allowLokaAllocRaw();
  }
}
#endif
