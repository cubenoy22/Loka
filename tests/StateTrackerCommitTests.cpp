#include "StateTrackerCommitTests.hpp"
#include <cassert>
#include <cstdio>
#include "core/State.hpp"
#include "core/StateTracker.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/boundary/Boundary.hpp"
#include "app/scene/projection/PlatformController.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/nodes/controls/Button.hpp"

namespace
{
  struct TrackerCommitProbe
  {
    TrackerCommitProbe()
        : tracker(0),
          initialState(0),
          commitWindowState(0),
          invalidations(0),
          deferredCalls(0)
    {
    }

    loka::core::PushStateTracker *tracker;
    loka::core::MutableState<int> *initialState;
    loka::core::MutableState<int> *commitWindowState;
    int invalidations;
    int deferredCalls;
  };

  bool containsState(const loka::core::PushStateTracker::StateList &states,
                     const loka::core::StateBase *state)
  {
    for (size_t i = 0; i < states.size(); ++i)
    {
      if (states[i] == state)
      {
        return true;
      }
    }
    return false;
  }

  void countDeferredCommitWork(void *userData)
  {
    TrackerCommitProbe *probe = static_cast<TrackerCommitProbe *>(userData);
    ++probe->deferredCalls;
  }

  void deferFromCommitWindowObserver(void *userData)
  {
    TrackerCommitProbe *probe = static_cast<TrackerCommitProbe *>(userData);
    probe->tracker->defer(&countDeferredCommitWork, probe);
  }

  void writeFromTrackerInvalidate(void *userData)
  {
    TrackerCommitProbe *probe = static_cast<TrackerCommitProbe *>(userData);
    ++probe->invalidations;
    const loka::core::PushStateTracker::StateList &committed =
        probe->tracker->committedDirtyStates();
    if (probe->invalidations == 1)
    {
      assert(containsState(committed, probe->initialState));
      assert(!containsState(committed, probe->commitWindowState));
      probe->commitWindowState->set(1);
      return;
    }
    assert(probe->invalidations == 2);
    assert(!containsState(committed, probe->initialState));
    assert(containsState(committed, probe->commitWindowState));
  }

  struct TrackerCommitLimitProbe
  {
    TrackerCommitLimitProbe()
        : state(0),
          invalidations(0)
    {
    }

    loka::core::MutableState<int> *state;
    int invalidations;
  };

  void keepWritingFromTrackerInvalidate(void *userData)
  {
    TrackerCommitLimitProbe *probe =
        static_cast<TrackerCommitLimitProbe *>(userData);
    ++probe->invalidations;
    probe->state->set(probe->state->get() + 1);
  }

  using namespace loka::app::scene;

  class CommitWindowBoundaryNode;
  typedef BoundaryPropsFor<CommitWindowBoundaryNode> CommitWindowBoundaryProps;
  CommitWindowBoundaryNode *g_commitWindowBoundary = 0;

  class CommitWindowBoundaryNode : public BoundaryNodeFor<CommitWindowBoundaryNode>
  {
  public:
    explicit CommitWindowBoundaryNode(const CommitWindowBoundaryProps &props)
        : BoundaryNodeFor<CommitWindowBoundaryNode>(props),
          shown_()
    {
      this->state(this->shown_, true);
      g_commitWindowBoundary = this;
    }

    virtual ~CommitWindowBoundaryNode()
    {
      if (g_commitWindowBoundary == this)
      {
        g_commitWindowBoundary = 0;
      }
    }

    virtual void composeNode(NodeComposition &composition)
    {
      using namespace loka::app;
      composition.declare(Show(*this->shown_.state()) << Button("commit-window"));
    }

    virtual void composeWithContext(ComponentContext &context, ComposeEvent event)
    {
      if (event == COMPOSE_EVENT_UPDATE && this->isApplyingPlatform())
      {
        ++updatesDuringApply;
      }
      BoundaryNodeFor<CommitWindowBoundaryNode>::composeWithContext(context, event);
    }

    void toggle()
    {
      this->shown_.set(!this->shown_.get());
    }

    static int updatesDuringApply;

  private:
    NodeState<bool> shown_;
  };

  int CommitWindowBoundaryNode::updatesDuringApply = 0;

  class CommitWindowPlatformController : public IPlatformController
  {
  public:
    CommitWindowPlatformController()
        : toggleOnNextChange(false),
          onChangeCalls(0),
          nestedCallsDuringToggle(-1)
    {
    }

    virtual void onChange(Node *, NodeDirtyFlags, bool)
    {
      ++this->onChangeCalls;
      if (!this->toggleOnNextChange || !g_commitWindowBoundary)
      {
        return;
      }
      this->toggleOnNextChange = false;
      const int callsBeforeToggle = this->onChangeCalls;
      g_commitWindowBoundary->toggle();
      this->nestedCallsDuringToggle = this->onChangeCalls - callsBeforeToggle;
    }

    virtual void synchronize() {}
    virtual bool hasPendingSync() const { return false; }
    virtual void destroy() {}

    bool toggleOnNextChange;
    int onChangeCalls;
    int nestedCallsDuringToggle;
  };
} // namespace

void testStateTrackerCommitQueuesNextTransaction()
{
  printf("\n==== [testStateTrackerCommitQueuesNextTransaction] start ====\n");
  // #60: a state write from invalidate belongs to a distinct next commit.
  loka::core::MutableState<int> initialState(0);
  loka::core::MutableState<int> commitWindowState(0);
  loka::core::PushStateTracker tracker;
  TrackerCommitProbe probe;
  probe.tracker = &tracker;
  probe.initialState = &initialState;
  probe.commitWindowState = &commitWindowState;
  tracker.addState(&initialState);
  tracker.addState(&commitWindowState);
  tracker.setInvalidateCallback(&writeFromTrackerInvalidate, &probe);
  commitWindowState.bind(&deferFromCommitWindowObserver, &probe, false);

  tracker.begin();
  initialState.set(1);
  const bool settled = tracker.end();

  assert(settled);
  assert(probe.invalidations == 2);
  assert(probe.deferredCalls == 1);
  assert(commitWindowState.get() == 1);
  assert(tracker.phase() == loka::core::TRACKER_IDLE);
  printf("==== [testStateTrackerCommitQueuesNextTransaction] end ====\n");
}

void testStateTrackerCommitWriteReachesNextSceneApply()
{
  printf("\n==== [testStateTrackerCommitWriteReachesNextSceneApply] start ====\n");
  // #60: the queued commit must reach Scene without re-entering platform apply.
  CommitWindowBoundaryNode::updatesDuringApply = 0;
  loka::app::scene::Scene scene(
      (loka::app::scene::Boundary<CommitWindowBoundaryNode>()));
  CommitWindowPlatformController platform;
  scene.mount(&platform);
  scene.updateAttached(true);
  assert(g_commitWindowBoundary != 0);

  const int callsBeforeWrite = platform.onChangeCalls;
  platform.toggleOnNextChange = true;
  g_commitWindowBoundary->toggle();
  scene.flushInvalidation();

  assert(!platform.toggleOnNextChange);
  assert(platform.nestedCallsDuringToggle == 0);
  assert(CommitWindowBoundaryNode::updatesDuringApply == 0);
  assert(platform.onChangeCalls == callsBeforeWrite + 2);

  scene.updateAttached(false);
  scene.unmount();
  printf("==== [testStateTrackerCommitWriteReachesNextSceneApply] end ====\n");
}

void testStateTrackerCommitChainReportsIterationLimit()
{
  printf("\n==== [testStateTrackerCommitChainReportsIterationLimit] start ====\n");
  loka::core::MutableState<int> state(0);
  loka::core::PushStateTracker tracker;
  TrackerCommitLimitProbe probe;
  probe.state = &state;
  tracker.addState(&state);
  tracker.setInvalidateCallback(&keepWritingFromTrackerInvalidate, &probe);

  tracker.begin();
  state.set(1);
  const bool settled = tracker.end();

  assert(!settled);
  assert(probe.invalidations == 1000);
  assert(state.get() == 1001);
  assert(tracker.phase() == loka::core::TRACKER_IDLE);
  printf("==== [testStateTrackerCommitChainReportsIterationLimit] end ====\n");
}
