#include "RectSurfaceSeatExtentTests.hpp"

#include "app/RectSurface.hpp"
#include "app/layout/LayoutHeuristics.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/ScrollView.hpp"
#include "core/StateTracker.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "support/TestVerify.hpp"

namespace
{
  struct ChangeCount
  {
    ChangeCount()
        : value(0)
    {
    }

    static void Increment(void *userData)
    {
      ChangeCount *count = static_cast<ChangeCount *>(userData);
      if (count)
      {
        ++count->value;
      }
    }

    int value;
  };

  loka::app::scene::LayoutState layoutState(short x, short y, short width, short height)
  {
    loka::app::scene::LayoutState state;
    state.x = x;
    state.y = y;
    state.width = width;
    state.height = height;
    state.lineHeight = 10;
    state.spacing = 0;
    return state;
  }

  void verifyFrame(const loka::core::Frame &actual, int x, int y, int width, int height)
  {
    LOKA_VERIFY(actual == loka::core::Frame(x, y, width, height));
  }

  loka::app::BoxNode *fixedBox(short width, short height)
  {
    loka::app::BoxProps props;
    props.setSize(width, height);
    return new loka::app::BoxNode(props);
  }

  struct LayoutSequence
  {
    LayoutSequence()
        : next(0),
          tail(0),
          publication(0)
    {
    }

    int next;
    int tail;
    int publication;
  };

  class LayoutTailNode : public loka::app::scene::Node, public loka::app::scene::IProjectedLayoutNode
  {
  public:
    explicit LayoutTailNode(LayoutSequence *sequence)
        : sequence_(sequence)
    {
    }

    virtual loka::app::scene::IProjectedLayoutNode *asProjectedLayoutNode()
    {
      return this;
    }

    virtual short layoutProjected(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state)
    {
      if (this->sequence_)
      {
        this->sequence_->tail = ++this->sequence_->next;
      }
      return state.y;
    }

  private:
    LayoutSequence *sequence_;
  };

  struct ReleaseContextOnExtent
  {
    ReleaseContextOnExtent()
        : surface(0),
          tailFinished(0),
          observedTailFinished(false),
          calls(0)
    {
    }

    static void OnExtent(void *userData)
    {
      ReleaseContextOnExtent *observer = static_cast<ReleaseContextOnExtent *>(userData);
      if (!observer)
      {
        return;
      }
      observer->observedTailFinished = observer->tailFinished && observer->tailFinished->tail > 0;
      ++observer->calls;
      if (observer->surface)
      {
        observer->surface->setContext(0);
      }
    }

    loka::app::RectSurfaceNode *surface;
    LayoutSequence *tailFinished;
    bool observedTailFinished;
    int calls;
  };

  void RecordExtentPublication(void *userData)
  {
    LayoutSequence *sequence = static_cast<LayoutSequence *>(userData);
    if (sequence)
    {
      sequence->publication = ++sequence->next;
    }
  }
} // namespace

void testRectSurfaceFillSeatReportsRowAllocation()
{
  loka::core::MutableState<loka::core::Frame> extent;
  loka::core::PushStateTracker tracker;
  tracker.addState(&extent);
  loka::app::scene::NodeState<loka::core::Frame> extentState(&extent, &tracker);
  loka::app::RectSurfaceProps surfaceProps;
  surfaceProps.laidOutExtent(extentState);

  loka::app::StackNode row((loka::app::StackProps(loka::app::STACK_AXIS_ROW)));
  row.addChild(fixedBox(200, 40));
  row.addChild(new loka::app::RectSurfaceNode(surfaceProps));

  NullScenePlatformController platform;
  platform.projectLayoutForTesting(&row, layoutState(0, 7, 500, 80));

  verifyFrame(extent.get(), 204, 7, 296, 80);
}

void testRectSurfaceExplicitSizeKeepsRowConsultationAndReportsDeclaredExtent()
{
  loka::core::MutableState<loka::core::Frame> extents[2];
  loka::core::PushStateTracker tracker;
  tracker.addState(&extents[0]);
  tracker.addState(&extents[1]);
  loka::app::scene::NodeState<loka::core::Frame> extentState(&extents[0], &tracker);
  loka::app::scene::NodeState<loka::core::Frame> otherExtentState(&extents[1], &tracker);
  loka::app::RectSurfaceProps surfaceProps;
  surfaceProps.size(100, 30).laidOutExtent(extentState);
  loka::app::RectSurfaceProps otherExtentProps;
  otherExtentProps.size(100, 30).laidOutExtent(otherExtentState);
  loka::app::RectSurfaceNode *surface = new loka::app::RectSurfaceNode(surfaceProps);

  loka::app::StackNode row((loka::app::StackProps(loka::app::STACK_AXIS_ROW)));
  row.addChild(fixedBox(200, 40));
  row.addChild(surface);

  LOKA_VERIFY(loka::app::layout::preferredChildWidthForRow(surface) == 100);
  LOKA_VERIFY(surfaceProps < otherExtentProps);

  NullScenePlatformController platform;
  platform.projectLayoutForTesting(&row, layoutState(0, 7, 500, 80));

  verifyFrame(extents[0].get(), 204, 7, 100, 30);
}

void testRectSurfaceFillSeatReportsColumnRemainingHeight()
{
  loka::core::MutableState<loka::core::Frame> extent;
  loka::core::PushStateTracker tracker;
  tracker.addState(&extent);
  loka::app::scene::NodeState<loka::core::Frame> extentState(&extent, &tracker);
  loka::app::RectSurfaceProps surfaceProps;
  surfaceProps.laidOutExtent(extentState);

  loka::app::StackNode column((loka::app::StackProps(loka::app::STACK_AXIS_COLUMN)));
  column.addChild(fixedBox(100, 40));
  column.addChild(new loka::app::RectSurfaceNode(surfaceProps));

  NullScenePlatformController platform;
  platform.projectLayoutForTesting(&column, layoutState(5, 9, 300, 100));

  verifyFrame(extent.get(), 5, 49, 300, 60);
}

void testRectSurfaceExtentChangesOnlyDuringRailLayout()
{
  loka::core::MutableState<loka::app::RectSurfaceModel> model;
  loka::core::MutableState<loka::core::Frame> extent;
  loka::core::PushStateTracker tracker;
  tracker.addState(&model);
  tracker.addState(&extent);
  loka::app::scene::NodeState<loka::core::Frame> extentState(&extent, &tracker);
  ChangeCount extentChanges;
  extent.bind(&ChangeCount::Increment, &extentChanges, false);

  loka::app::RectSurfaceProps surfaceProps;
  surfaceProps.model(&model).size(120, 70).laidOutExtent(extentState);
  loka::app::RectSurfaceNode surface(surfaceProps);
  NullScenePlatformController platform;
  const loka::app::scene::LayoutState state = layoutState(3, 6, 120, 70);

  platform.projectLayoutForTesting(&surface, state);
  verifyFrame(extent.get(), 3, 6, 120, 70);
  LOKA_VERIFY(extentChanges.value == 1);

  platform.projectLayoutForTesting(&surface, state);
  LOKA_VERIFY(extentChanges.value == 1);

  const loka::core::Frame sentinel(1, 2, 3, 4);
  {
    loka::core::StateTrackerGuard guard(&tracker);
    extent.set(sentinel);
  }
  LOKA_VERIFY(extentChanges.value == 2);

  {
    loka::core::StateTrackerGuard guard(&tracker);
    loka::app::RectSurfaceModel changed;
    changed.rectCount = 1;
    changed.rects[0] = loka::app::RectSprite(2, 3, 4, 5);
    model.set(changed);
  }
  surface.render(&platform);

  LOKA_VERIFY(extent.get() == sentinel);
  LOKA_VERIFY(extentChanges.value == 2);
}

void testRectSurfaceWithoutExtentStateLaysOutNormally()
{
  loka::app::RectSurfaceNode surface((loka::app::RectSurfaceProps()));
  NullScenePlatformController platform;

  const int resultY = platform.projectLayoutForTesting(&surface, layoutState(3, 6, 120, 70));

  LOKA_VERIFY(surface.getContext() != 0);
  LOKA_VERIFY(resultY == 76);
}

void testRectSurfaceNodeStatePublicationUsesOwnerTracker()
{
  loka::core::MutableState<loka::core::Frame> extent;
  loka::core::PushStateTracker tracker;
  tracker.addState(&extent);
  loka::app::scene::NodeState<loka::core::Frame> extentState(&extent, &tracker);
  extentState.set(loka::core::Frame(1, 2, 3, 4));
  const bool controlDirty = tracker.consumeDirty();
  LOKA_VERIFY(controlDirty);
  loka::app::RectSurfaceProps props;
  props.laidOutExtent(extentState);
  loka::app::RectSurfaceNode surface(props);
  NullScenePlatformController platform;
  loka::app::scene::LayoutState seat;
  seat.x = 3;
  seat.y = 6;
  seat.width = 120;
  seat.height = 70;
  seat.lineHeight = 10;
  seat.spacing = 0;
  platform.projectLayoutForTesting(&surface, seat);
  LOKA_VERIFY(extent.get() == loka::core::Frame(3, 6, 120, 70));
  const bool publishedDirty = tracker.consumeDirty();
  LOKA_VERIFY(publishedDirty);
}

void testRectSurfaceExtentPublicationCanReleaseContextAfterLayout()
{
  loka::core::MutableState<loka::core::Frame> extent;
  loka::core::PushStateTracker tracker;
  tracker.addState(&extent);
  loka::app::scene::NodeState<loka::core::Frame> extentState(&extent, &tracker);
  loka::app::RectSurfaceProps props;
  props.size(120, 70).laidOutExtent(extentState);
  loka::app::RectSurfaceNode *surface = new loka::app::RectSurfaceNode(props);
  LayoutSequence sequence;
  ReleaseContextOnExtent observer;
  observer.surface = surface;
  observer.tailFinished = &sequence;
  extent.bind(&ReleaseContextOnExtent::OnExtent, &observer, false);

  loka::app::StackNode row((loka::app::StackProps(loka::app::STACK_AXIS_ROW)));
  row.addChild(surface);
  row.addChild(new LayoutTailNode(&sequence));
  NullScenePlatformController platform;
  platform.projectLayoutForTesting(&row, layoutState(3, 6, 240, 70));

  LOKA_VERIFY(observer.calls == 1);
  LOKA_VERIFY(observer.observedTailFinished);
  LOKA_VERIFY(surface->getContext() == 0);
}

void testRectSurfaceExtentPublicationFollowsLayoutTraversal()
{
  loka::core::MutableState<loka::core::Frame> extent;
  loka::core::PushStateTracker tracker;
  tracker.addState(&extent);
  loka::app::scene::NodeState<loka::core::Frame> extentState(&extent, &tracker);
  loka::app::RectSurfaceProps props;
  props.size(120, 70).laidOutExtent(extentState);
  LayoutSequence sequence;
  extent.bind(&RecordExtentPublication, &sequence, false);

  loka::app::StackNode row((loka::app::StackProps(loka::app::STACK_AXIS_ROW)));
  row.addChild(new loka::app::RectSurfaceNode(props));
  row.addChild(new LayoutTailNode(&sequence));
  NullScenePlatformController platform;
  platform.projectLayoutForTesting(&row, layoutState(3, 6, 240, 70));

  LOKA_VERIFY(sequence.tail > 0);
  LOKA_VERIFY(sequence.publication > sequence.tail);
}

void testRectSurfaceScrollViewExtentUsesContentCoordinates()
{
  loka::core::MutableState<int> offset(7);
  loka::core::MutableState<loka::core::Frame> extent;
  loka::core::PushStateTracker tracker;
  tracker.addState(&offset);
  tracker.addState(&extent);
  loka::app::scene::NodeState<int> offsetState(&offset, &tracker);
  loka::app::scene::NodeState<loka::core::Frame> extentState(&extent, &tracker);
  loka::app::RectSurfaceProps props;
  props.laidOutExtent(extentState);
  loka::app::ScrollViewNode scrollView((loka::app::ScrollViewProps(offsetState)));
  scrollView.addChild(new loka::app::RectSurfaceNode(props));

  NullScenePlatformController platform;
  platform.projectLayoutForTesting(&scrollView, layoutState(10, 20, 100, 40));

  verifyFrame(extent.get(), 10, 20, 100, 40);
}

namespace
{
  // A watcher on the first surface's fact records a newer extent for the
  // second surface and flushes it (a nested layout pass), while the outer
  // flush still holds the second surface's older entry.
  struct NestedLedgerRecorder
  {
    NestedLedgerRecorder()
        : ledger(0),
          later(0),
          calls(0)
    {
    }

    static void OnFirstExtent(void *userData)
    {
      NestedLedgerRecorder *self = static_cast<NestedLedgerRecorder *>(userData);
      if (!self || !self->ledger || !self->later)
      {
        return;
      }
      ++self->calls;
      if (self->calls == 1)
      {
        self->ledger->record(self->later, loka::core::Frame(0, 0, 20, 20));
        self->ledger->flush();
      }
    }

    loka::app::RectSurfaceExtentLedger *ledger;
    loka::app::RectSurfaceNode *later;
    int calls;
  };
} // namespace

void testRectSurfaceExtentLedgerNestedFlushKeepsNewerEntry()
{
  loka::core::MutableState<loka::core::Frame> firstExtent;
  loka::core::MutableState<loka::core::Frame> laterExtent;
  loka::core::PushStateTracker tracker;
  tracker.addState(&firstExtent);
  tracker.addState(&laterExtent);
  loka::app::scene::NodeState<loka::core::Frame> firstState(&firstExtent, &tracker);
  loka::app::scene::NodeState<loka::core::Frame> laterState(&laterExtent, &tracker);
  loka::app::RectSurfaceProps firstProps;
  firstProps.laidOutExtent(firstState);
  loka::app::RectSurfaceProps laterProps;
  laterProps.laidOutExtent(laterState);
  loka::app::RectSurfaceNode first(firstProps);
  loka::app::RectSurfaceNode later(laterProps);

  loka::app::RectSurfaceExtentLedger ledger;
  NestedLedgerRecorder recorder;
  recorder.ledger = &ledger;
  recorder.later = &later;
  firstExtent.bind(&NestedLedgerRecorder::OnFirstExtent, &recorder, false);

  ledger.record(&first, loka::core::Frame(0, 0, 10, 10));
  ledger.record(&later, loka::core::Frame(0, 0, 2, 2));
  ledger.flush();

  LOKA_VERIFY(recorder.calls == 1);
  LOKA_VERIFY(firstExtent.get() == loka::core::Frame(0, 0, 10, 10));
  LOKA_VERIFY(laterExtent.get() == loka::core::Frame(0, 0, 20, 20));
}

// A refused layout (ScrollView offset beyond the short range) places nothing,
// so no seat fact is published for the surface.
void testRectSurfaceRefusedProjectionPublishesNoExtent()
{
  loka::core::MutableState<int> offset(40000);
  loka::core::MutableState<loka::core::Frame> extent(loka::core::Frame(1, 2, 3, 4));
  loka::core::PushStateTracker tracker;
  tracker.addState(&offset);
  tracker.addState(&extent);
  loka::app::scene::NodeState<int> offsetState(&offset, &tracker);
  loka::app::scene::NodeState<loka::core::Frame> extentState(&extent, &tracker);
  loka::app::RectSurfaceProps props;
  props.laidOutExtent(extentState);
  loka::app::ScrollViewNode scrollView((loka::app::ScrollViewProps(offsetState)));
  scrollView.addChild(new loka::app::RectSurfaceNode(props));

  NullScenePlatformController platform;
  platform.projectLayoutForTesting(&scrollView, layoutState(10, 20, 100, 40));

  verifyFrame(extent.get(), 1, 2, 3, 4);
}
