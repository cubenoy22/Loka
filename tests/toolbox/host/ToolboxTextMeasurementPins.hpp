#ifndef LOKA_TOOLBOX_TEXT_MEASUREMENT_PINS_HPP
#define LOKA_TOOLBOX_TEXT_MEASUREMENT_PINS_HPP
#include "context/ToolboxTextContext.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "support/LifecycleFactTestAccess.hpp"

namespace loka
{
  namespace testing
  {
    class ToolboxTextContextAccess
    {
    public:
      static Rect rect(const ToolboxTextContext &c)
      {
        return c.rect_;
      }
      static Rect paintRect(const ToolboxTextContext &c)
      {
        return c.paintRect_;
      }
      static short baseline(const ToolboxTextContext &c)
      {
        return c.textY_;
      }
    };
  } // namespace testing
} // namespace loka
namespace
{
  using namespace loka::core;
  class MeasurementBoundary : public BoundaryNodeFor<MeasurementBoundary>
  {
  public:
    MeasurementBoundary(const BoundaryPropsFor<MeasurementBoundary> &p = BoundaryPropsFor<MeasurementBoundary>())
        : BoundaryNodeFor<MeasurementBoundary>(p)
    {
    }
    virtual void composeNode(NodeComposition &) {}
  };
  int MeasurementCalls()
  {
    return toolbox_host::metrics + toolbox_host::widths + toolbox_host::measures;
  }
  Rect MeasurementRect(const ToolboxTextContext &c)
  {
    return loka::testing::ToolboxTextContextAccess::rect(c);
  }
  Rect MeasurementRect(const ToolboxAttributedTextContext &c)
  {
    return ToolboxAttributedTextContextAccess::rect(c);
  }
  Rect MeasurementClip(const ToolboxTextContext &c)
  {
    return loka::testing::ToolboxTextContextAccess::paintRect(c);
  }
  Rect MeasurementClip(const ToolboxAttributedTextContext &c)
  {
    return ToolboxAttributedTextContextAccess::paintRect(c);
  }

  void RepaintRefused(ToolboxTextContext &context, ToolboxScenePlatformController &)
  {
    context.repaint();
  }
  void RepaintRefused(ToolboxAttributedTextContext &context, ToolboxScenePlatformController &controller)
  {
    context.render(&controller);
  }

  template <class Context, class NodeType>
  void RepeatWidthPlacementAndRefusal(NodeType &node, Context &context, ToolboxScenePlatformController &controller)
  {
    LayoutState seat = Seat(40);
    seat.lineHeight = 14;
    toolbox_host::reset();
    node.layout(&controller, seat);
    LOKA_VERIFY(MeasurementCalls() > 0);
    const Rect original = MeasurementRect(context);
    const int first = MeasurementCalls();
    seat = Seat(40);
    seat.lineHeight = 14;
    seat.y = 70;
    seat.x = 17;
    seat.spacing = 7;
    SetRect(&controller.projectionClip, 19, 72, 35, 78);
    node.layout(&controller, seat);
    LOKA_VERIFY(MeasurementCalls() == first);
    const Rect moved = MeasurementRect(context);
    LOKA_VERIFY(moved.top == 70 && moved.left == 17);
    LOKA_VERIFY(moved.bottom - moved.top == original.bottom - original.top);
    LOKA_VERIFY(seat.y == moved.bottom + 7);
    const Rect clipped = MeasurementClip(context);
    LOKA_VERIFY(clipped.left == 19 && clipped.top == 72 && clipped.right == 35 && clipped.bottom == 78);
    SetRect(&controller.projectionClip, -30000, -30000, 30000, 30000);
    seat = Seat(25);
    seat.lineHeight = 14;
    node.layout(&controller, seat);
    LOKA_VERIFY(MeasurementCalls() > first);

    // Intrinsic output must not replace the zero/nonpositive input key.
    seat = Seat(0);
    seat.lineHeight = 14;
    const short intrinsic = node.layout(&controller, seat);
    LOKA_VERIFY(intrinsic > 0);
    int before = MeasurementCalls();
    seat = Seat(0);
    seat.lineHeight = 14;
    LOKA_VERIFY(node.layout(&controller, seat) == intrinsic);
    LOKA_VERIFY(MeasurementCalls() == before);
    seat = Seat(-1);
    seat.lineHeight = 14;
    node.layout(&controller, seat);
    LOKA_VERIFY(MeasurementCalls() > before);

    NotifySubtreeNodeDetached(&node);
    LifecycleFactTestAccess::DeliverFacts(&node);
    NotifySubtreeNodeAttached(&node);
    LifecycleFactTestAccess::DeliverFacts(&node);
    before = MeasurementCalls();
    seat = Seat(-1);
    seat.lineHeight = 14;
    node.layout(&controller, seat);
    LOKA_VERIFY(MeasurementCalls() > before);
    before = MeasurementCalls();
    seat = Seat(-1);
    seat.lineHeight = 14;
    node.layout(&controller, seat);
    LOKA_VERIFY(MeasurementCalls() == before);

    // Refuse after a completed result, at the SAME constraint. The input mark
    // must survive refusal along with a queued controller retry.
    NotifySubtreeNodeAttached(&node);
    LifecycleFactTestAccess::DeliverFacts(&node);
    ToolboxWindow *window = controller.window_;
    controller.window_ = 0;
    seat = Seat(-1);
    seat.lineHeight = 14;
    seat.x = 80;
    seat.y = 90;
    LOKA_VERIFY(node.layout(&controller, seat) == 0);
    LOKA_VERIFY(controller.relayoutRetries.pending());
    for (int retry = 0; retry != 3; ++retry)
    {
      controller.relayoutRetries.flush(controller, node, seat);
      LOKA_VERIFY(controller.relayoutRetries.pending());
    }
    controller.window_ = window;
    // Recovery can precede the retry layout. Refusal must revoke the old seat,
    // including the inactive-clip fallback, before any paint/hit can observe it.
    const int hits = toolbox_host::textHits;
    for (int regions = 0; regions <= 2; regions += 2)
    {
      toolbox_host::draws.clear();
      toolbox_host::failRegions = regions;
      context.render(&controller);
      RepaintRefused(context, controller);
      LOKA_VERIFY(toolbox_host::draws.empty());
      LOKA_VERIFY(toolbox_host::textHits == hits);
      const PaintQuery query = {ToolboxPaintScope(), PLACEMENT_ELIGIBLE};
      LOKA_VERIFY(context.queryPaintDamage(query).kind == PAINT_ANSWER_REFUSED);
    }
    toolbox_host::failRegions = 0;
    before = MeasurementCalls();
    seat = Seat(-1);
    seat.lineHeight = 14;
    controller.relayoutRetries.flush(controller, node, seat);
    LOKA_VERIFY(!controller.relayoutRetries.pending());
    LOKA_VERIFY(MeasurementCalls() > before);
    before = MeasurementCalls();
    seat = Seat(-1);
    seat.lineHeight = 14;
    node.layout(&controller, seat);
    LOKA_VERIFY(MeasurementCalls() == before);
    std::printf("measurement pin: first=%d retry=%d hit=0\n", first, before);
  }

  void MeasurementPins(bool plainPins, bool attributedPins)
  {
    ToolboxWindow window;
    ToolboxScenePlatformController controller(&window);
    LOKA_VERIFY(RegisterToolboxBuiltInSupport(controller));
    MeasurementBoundary owner;
    MutableState<String> plain(String::Literal("abcdefgh ijkl"));
    MutableState<TextStyle> style((TextStyle()));
    MutableState<AttributedString> attributed(Styled("abcdefgh ijkl", FontSize<12>()));
    PushStateTracker tracker;
    tracker.addState(&plain);
    tracker.addState(&style);
    tracker.addState(&attributed);
    if (plainPins)
    {
      TextDefinitionWithAttr definition = Text(&plain) + &style;
      TextNode *node = static_cast<TextNode *>(definition.create());
      LOKA_VERIFY(node);
      LayoutState seat = Seat(40);
      NodeContext *installed = controller.nodeHandlerRegistry_.find(node)->ensureContext(node, &controller, seat);
      LOKA_VERIFY(installed);
      ToolboxTextContext &context = *static_cast<ToolboxTextContext *>(installed);
      Pin("PlainMeasurementReusePlacementWidthParkingRefusal");
      RepeatWidthPlacementAndRefusal(*node, context, controller);
      BoundaryNode::declareBoundaryDirtySources(node, &owner);
      for (int route = 0; route < 3; ++route)
      {
        seat = Seat(40);
        seat.lineHeight = 14;
        node->layout(&controller, seat);
        const int before = MeasurementCalls();
        if (route == 0)
        {
          StateTrackerGuard guard(&tracker);
          plain.set(String::Literal("new content"));
        }
        if (route == 1)
        {
          StateTrackerGuard guard(&tracker);
          style.set(FontSize<24>());
        }
        if (route == 2)
        {
          TextDefinitionWithAttr replacement = Text(&plain) + &style + BlockStyle().wrap(TEXT_WRAP_CHAR);
          LOKA_VERIFY(replacement.applyPropsToNode(node));
        }
        seat = Seat(40);
        seat.lineHeight = 14;
        node->layout(&controller, seat);
        LOKA_VERIFY(MeasurementCalls() > before);
      }
      Pin("PlainWrappedGeometryBothPolicies");
      for (int wrap = 0; wrap < 2; ++wrap)
      {
        TextDefinitionWithAttr wrapped =
            Text("abcdefgh") + FontSize<12>() + BlockStyle().wrap(wrap == 0 ? TEXT_WRAP_CHAR : TEXT_WRAP_WORD);
        LOKA_VERIFY(wrapped.applyPropsToNode(node));
        seat = Seat(20);
        seat.lineHeight = 14;
        node->layout(&controller, seat);
        const Rect rect = MeasurementRect(context);
        LOKA_VERIFY(rect.bottom - rect.top == 34);
        LOKA_VERIFY(loka::testing::ToolboxTextContextAccess::baseline(context) == 32);
        const int before = MeasurementCalls();
        seat = Seat(20);
        seat.lineHeight = 14;
        node->layout(&controller, seat);
        LOKA_VERIFY(MeasurementCalls() == before);
      }
      // No explicit size: lineHeight changes both height and baseline.
      Text unset = Text(&plain);
      LOKA_VERIFY(unset.applyPropsToNode(node));
      seat = Seat(40);
      seat.lineHeight = 14;
      node->layout(&controller, seat);
      const short baseline = loka::testing::ToolboxTextContextAccess::baseline(context);
      int before = MeasurementCalls();
      seat = Seat(40);
      seat.lineHeight = 20;
      node->layout(&controller, seat);
      LOKA_VERIFY(MeasurementCalls() > before);
      LOKA_VERIFY(loka::testing::ToolboxTextContextAccess::baseline(context) == baseline + 6);
      Pin("PlainMeasurementHitStillFitsEllipsis");
      TextDefinitionWithAttr ellipsis = Text("abcdefgh") + BlockStyle().truncation(TEXT_TRUNCATION_ELLIPSIS);
      LOKA_VERIFY(ellipsis.applyPropsToNode(node));
      seat = Seat(25);
      node->layout(&controller, seat);
      before = MeasurementCalls();
      seat = Seat(25);
      node->layout(&controller, seat);
      LOKA_VERIFY(MeasurementCalls() == before);
      toolbox_host::draws.clear();
      context.render(&controller);
      LOKA_VERIFY(MeasurementCalls() > before);
      LOKA_VERIFY(toolbox_host::draws.size() == 1 && toolbox_host::draws[0].bytes == "abc...");
      DestroyHeapNode(node);
    }
    owner.clearObservedStateEntries();
    if (attributedPins)
    {
      AttributedText definition(&attributed);
      AttributedTextNode *node = static_cast<AttributedTextNode *>(definition.create());
      LOKA_VERIFY(node);
      LayoutState seat = Seat(40);
      NodeContext *installed = controller.nodeHandlerRegistry_.find(node)->ensureContext(node, &controller, seat);
      LOKA_VERIFY(installed);
      ToolboxAttributedTextContext &context = *static_cast<ToolboxAttributedTextContext *>(installed);
      Pin("AttributedMeasurementReusePlacementWidthParkingRefusal");
      RepeatWidthPlacementAndRefusal(*node, context, controller);
      BoundaryNode::declareBoundaryDirtySources(node, &owner);
      for (int route = 0; route < 3; ++route)
      {
        seat = Seat(40);
        node->layout(&controller, seat);
        const int before = MeasurementCalls();
        if (route < 2)
        {
          StateTrackerGuard guard(&tracker);
          attributed.set(route == 0 ? Styled("new content", FontSize<12>()) : Styled("new content", FontSize<24>()));
        }
        else
        {
          AttributedTextDefinitionWithAttr replacement =
              AttributedText(&attributed) + BlockStyle().wrap(TEXT_WRAP_CHAR);
          LOKA_VERIFY(replacement.applyPropsToNode(node));
        }
        seat = Seat(40);
        node->layout(&controller, seat);
        LOKA_VERIFY(MeasurementCalls() > before);
      }
      DestroyHeapNode(node);
    }
    owner.clearObservedStateEntries();
  }
} // namespace
#endif
