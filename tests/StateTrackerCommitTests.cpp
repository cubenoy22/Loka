#include "testing/core/StateTrackerTestAccess.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "StateTrackerCommitTests.hpp"
#include <cassert>
#include <cstdio>
#include <climits>
#include "core/State.hpp"
#include "core/StateTracker.hpp"
#include "core/util/StateTrackerGuard.hpp"
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
    loka::core::testing::PushStateTrackerTestAccess::defer(*probe->tracker, &countDeferredCommitWork, probe);
  }

  void writeFromTrackerInvalidate(void *userData)
  {
    TrackerCommitProbe *probe = static_cast<TrackerCommitProbe *>(userData);
    ++probe->invalidations;
    const loka::core::PushStateTracker::StateList &committed =
        probe->tracker->committedDirtyStates();
    if (probe->invalidations == 1)
    {
      (void)committed;
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

  class IncrementedStateEval : public loka::core::DerivedState<int>::EvalFn
  {
  public:
    explicit IncrementedStateEval(loka::core::State<int> *source)
        : source_(source)
    {
    }

    virtual int operator()()
    {
      return this->source_->get() + 1;
    }

  private:
    loka::core::State<int> *source_;
  };

  void incrementGuardInvalidations(void *userData)
  {
    int *invalidations = static_cast<int *>(userData);
    ++*invalidations;
  }

  struct SettlingGuardProbe
  {
    SettlingGuardProbe()
        : tracker(0),
          derived(0),
          written(0),
          invalidations(0)
    {
    }

    loka::core::PushStateTracker *tracker;
    loka::core::State<int> *derived;
    loka::core::MutableState<int> *written;
    int invalidations;
  };

  void writeFromSettlingDerived(void *userData)
  {
    SettlingGuardProbe *probe = static_cast<SettlingGuardProbe *>(userData);
    {
      loka::core::StateTrackerGuard guard(
          probe->tracker, &incrementGuardInvalidations, &probe->invalidations);
      probe->written->set(probe->derived->get());
    }
    // A joined level is not a closed transaction: nothing may acknowledge the
    // outer settlement's dirt from inside it (MenuComposition::declare() is the
    // manual begin/end/peekDirty client this protects).
    assert(!probe->tracker->peekDirty());
    assert(!probe->tracker->consumeDirty());
  }

  struct CommitGuardProbe
  {
    CommitGuardProbe()
        : tracker(0),
          source(0),
          written(0),
          invalidations(0)
    {
    }

    loka::core::PushStateTracker *tracker;
    loka::core::State<int> *source;
    loka::core::MutableState<int> *written;
    int invalidations;
  };

  void writeFromDeferredCommit(void *userData)
  {
    CommitGuardProbe *probe = static_cast<CommitGuardProbe *>(userData);
    {
      loka::core::StateTrackerGuard guard(
          probe->tracker, &incrementGuardInvalidations, &probe->invalidations);
      probe->written->set(probe->source->get());
    }
    assert(!probe->tracker->peekDirty());
    assert(!probe->tracker->consumeDirty());
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
  (void)&containsState;
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

  (void)settled;
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
  loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
  assert(g_commitWindowBoundary != 0);

  const int callsBeforeWrite = platform.onChangeCalls;
  platform.toggleOnNextChange = true;
  g_commitWindowBoundary->toggle();
  scene.flushInvalidation();

  assert(!platform.toggleOnNextChange);
  assert(platform.nestedCallsDuringToggle == 0);
  assert(CommitWindowBoundaryNode::updatesDuringApply == 0);
  (void)callsBeforeWrite;
  assert(platform.onChangeCalls == callsBeforeWrite + 2);

  loka::dsl::testing::SceneTestAccess::updateAttached(scene, false);
  loka::dsl::testing::SceneTestAccess::unmount(scene);
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

  (void)settled;
  assert(!settled);
  assert(probe.invalidations == 1000);
  assert(state.get() == 1001);
  assert(tracker.phase() == loka::core::TRACKER_IDLE);
  printf("==== [testStateTrackerCommitChainReportsIterationLimit] end ====\n");
}

void testStateTrackerGuardOpenedDuringSettlementJoinsTransaction()
{
  printf("\n==== [testStateTrackerGuardOpenedDuringSettlementJoinsTransaction] start ====\n");
  loka::core::MutableState<int> a(1);
  loka::core::DerivedState<int> d(&a, new IncrementedStateEval(&a));
  loka::core::MutableState<int> b(1);
  loka::core::DerivedState<int> e(&b, new IncrementedStateEval(&b));
  loka::core::PushStateTracker tracker;
  SettlingGuardProbe probe;
  probe.tracker = &tracker;
  probe.derived = &d;
  probe.written = &b;
  tracker.addState(&a);
  tracker.addState(&d);
  tracker.addState(&b);
  tracker.addState(&e);
  d.bind(&writeFromSettlingDerived, &probe, false);

  {
    loka::core::StateTrackerGuard guard(
        &tracker, &incrementGuardInvalidations, &probe.invalidations);
    a.set(4);
  }

  assert(d.get() == 5);
  assert(b.get() == 5);
  assert(e.get() == 6);
  assert(probe.invalidations == 1);
  assert(tracker.phase() == loka::core::TRACKER_IDLE);
  // The dirt the joined level could not consume is still pending for the owner.
  assert(tracker.peekDirty());
  assert(tracker.consumeDirty());
  assert(!tracker.peekDirty());

  {
    loka::core::StateTrackerGuard guard(&tracker);
    a.set(7);
  }

  assert(d.get() == 8);
  assert(e.get() == 9);
  assert(tracker.phase() == loka::core::TRACKER_IDLE);
  printf("==== [testStateTrackerGuardOpenedDuringSettlementJoinsTransaction] end ====\n");
}

void testStateTrackerGuardOpenedDuringCommitJoinsTransaction()
{
  printf("\n==== [testStateTrackerGuardOpenedDuringCommitJoinsTransaction] start ====\n");
  loka::core::MutableState<int> a(1);
  loka::core::MutableState<int> b(1);
  loka::core::DerivedState<int> e(&b, new IncrementedStateEval(&b));
  loka::core::PushStateTracker tracker;
  CommitGuardProbe probe;
  probe.tracker = &tracker;
  probe.source = &a;
  probe.written = &b;
  tracker.addState(&a);
  tracker.addState(&b);
  tracker.addState(&e);

  {
    loka::core::StateTrackerGuard guard(
        &tracker, &incrementGuardInvalidations, &probe.invalidations);
    a.set(10);
    loka::core::testing::PushStateTrackerTestAccess::defer(tracker, &writeFromDeferredCommit, &probe);
  }

  assert(b.get() == 10);
  assert(e.get() == 11);
  assert(probe.invalidations == 1);
  assert(tracker.phase() == loka::core::TRACKER_IDLE);
  printf("==== [testStateTrackerGuardOpenedDuringCommitJoinsTransaction] end ====\n");
}

namespace
{
  using loka::core::PushStateTracker;
  using loka::core::TrackerGeneration;
  using loka::core::testing::PushStateTrackerTestAccess;

  const TrackerGeneration staticTerminal = TrackerGeneration::terminal();

  struct GenerationChainProbe
  {
    GenerationChainProbe(PushStateTracker &owner, loka::core::MutableState<int> &value,
                         int rounds, const TrackerGeneration *tokens)
        : tracker(owner), state(value), stopAfter(rounds), expected(tokens), calls(0)
    {
    }

    PushStateTracker &tracker;
    loka::core::MutableState<int> &state;
    const int stopAfter;
    const TrackerGeneration *const expected;
    int calls;

    static void invalidate(void *data)
    {
      GenerationChainProbe &self = *static_cast<GenerationChainProbe *>(data);
      if (self.expected)
        assert(self.tracker.generation() == self.expected[self.calls]);
      ++self.calls;
      if (self.calls < self.stopAfter)
        self.state.set(self.state.get() + 1);
    }
  };

  void checkGenerationInReentrantGuard(void *data)
  {
    PushStateTracker &tracker = *static_cast<PushStateTracker *>(data);
    const TrackerGeneration before = tracker.generation();
    (void)before;
    assert(tracker.phase() != loka::core::TRACKER_IDLE);
    {
      loka::core::StateTrackerGuard guard(&tracker);
      assert(tracker.generation() == before);
    }
    assert(tracker.generation() == before);
  }

  void runGenerationLimitChain(PushStateTracker &tracker,
                               loka::core::MutableState<int> &state)
  {
    GenerationChainProbe probe(tracker, state, 1001, 0);
    tracker.addState(&state);
    tracker.setInvalidateCallback(&GenerationChainProbe::invalidate, &probe);
    tracker.begin();
    state.set(1);
    const bool settled = tracker.end();
    (void)settled;
    assert(!settled);
    assert(probe.calls == 1000);
    assert(state.get() == 1001);
    assert(tracker.phase() == loka::core::TRACKER_IDLE);
    assert(PushStateTrackerTestAccess::nextDirtyCount(tracker) == 1);
    tracker.setInvalidateCallback(0, 0);
  }
}

void testB1GenerationFreshBegin()
{
  // Rule: each outermost begin advances exactly once.
  PushStateTracker tracker;
  tracker.begin();
  const TrackerGeneration first = tracker.generation();
  (void)first;
  assert(first == PushStateTrackerTestAccess::generationValue(1));
  assert(first != TrackerGeneration::terminal());
  tracker.end();
  assert(tracker.generation() == first);
  tracker.begin();
  assert(tracker.generation() != first);
  assert(tracker.generation() == PushStateTrackerTestAccess::generationValue(2));
  assert(tracker.generation() != TrackerGeneration::terminal());
  tracker.end();
}

void testB1GenerationNestedBegin()
{
  // Rule: nested begin returns without advancing.
  PushStateTracker tracker;
  tracker.begin();
  const TrackerGeneration outer = tracker.generation();
  (void)outer;
  tracker.begin();
  assert(tracker.generation() == outer);
  tracker.end();
  assert(tracker.generation() == outer);
  tracker.end();
  assert(tracker.generation() == outer);
}

void testB1GenerationReentrantBegin()
{
  // Rule: a guard joining settlement returns without advancing.
  loka::core::MutableState<int> source(0);
  loka::core::DerivedState<int> derived(&source, new IncrementedStateEval(&source));
  PushStateTracker tracker;
  tracker.addState(&source);
  tracker.addState(&derived);
  derived.bind(&checkGenerationInReentrantGuard, &tracker, false);
  tracker.begin();
  const TrackerGeneration outer = tracker.generation();
  (void)outer;
  source.set(1);
  PushStateTrackerTestAccess::defer(tracker, &checkGenerationInReentrantGuard, &tracker);
  const bool settled = tracker.end();
  (void)settled;
  assert(settled);
  assert(derived.get() == 2);
  assert(tracker.generation() == outer);
}

void testB1GenerationNextIntake()
{
  // Rule: each successful next-to-current transfer advances exactly once.
  loka::core::MutableState<int> state(0);
  PushStateTracker tracker;
  const TrackerGeneration expected[] = {
      PushStateTrackerTestAccess::generationValue(1),
      PushStateTrackerTestAccess::generationValue(2),
      PushStateTrackerTestAccess::generationValue(3)};
  GenerationChainProbe probe(tracker, state, 3, expected);
  tracker.addState(&state);
  tracker.setInvalidateCallback(&GenerationChainProbe::invalidate, &probe);
  tracker.begin();
  state.set(1);
  const bool settled = tracker.end();
  (void)settled;
  assert(settled);
  assert(probe.calls == 3);
  assert(tracker.generation() == expected[2]);
}

void testB1GenerationLimitExit()
{
  // Rule: the failed final COMMIT round does not transfer or advance.
  loka::core::MutableState<int> state(0);
  PushStateTracker tracker;
  runGenerationLimitChain(tracker, state);
  assert(tracker.generation() == PushStateTrackerTestAccess::generationValue(1000));
  tracker.begin();
  assert(tracker.generation() == PushStateTrackerTestAccess::generationValue(1001));
  const bool settled = tracker.end();
  (void)settled;
  assert(settled);
}

void testB1GenerationLimitBeginNoDoubleAdvance()
{
  // Rule: clearing abandoned next work does not add another begin advance.
  loka::core::MutableState<int> state(0);
  PushStateTracker tracker;
  runGenerationLimitChain(tracker, state);
  tracker.begin();
  assert(PushStateTrackerTestAccess::nextDirtyCount(tracker) == 0);
  assert(tracker.generation() == PushStateTrackerTestAccess::generationValue(1001));
  tracker.end();
}

void testB1GenerationTerminalBegin()
{
  // Rule: begin uses the guarded advance, including while already TERMINAL.
  PushStateTracker tracker;
  PushStateTrackerTestAccess::seedGeneration(tracker, ULONG_MAX - 1);
  assert(tracker.generation() != TrackerGeneration::terminal());
  tracker.begin();
  assert(tracker.generation() == TrackerGeneration::terminal());
  tracker.end();
  tracker.begin();
  assert(tracker.generation() == TrackerGeneration::terminal());
  tracker.end();
}

void testB1GenerationTerminalAdvance()
{
  // Rule: next intake uses the same saturating procedure as outer begin.
  loka::core::MutableState<int> state(0);
  PushStateTracker tracker;
  PushStateTrackerTestAccess::seedGeneration(tracker, ULONG_MAX - 3);
  const TrackerGeneration expected[] = {
      PushStateTrackerTestAccess::generationValue(ULONG_MAX - 2),
      PushStateTrackerTestAccess::generationValue(ULONG_MAX - 1),
      TrackerGeneration::terminal(), TrackerGeneration::terminal()};
  GenerationChainProbe probe(tracker, state, 4, expected);
  tracker.addState(&state);
  tracker.setInvalidateCallback(&GenerationChainProbe::invalidate, &probe);
  tracker.begin();
  state.set(1);
  const bool settled = tracker.end();
  (void)settled;
  assert(settled);
  assert(probe.calls == 4);
  assert(tracker.generation() == TrackerGeneration::terminal());
}

void testB1GenerationTerminalStaticInitialization()
{
  // Rule: an exhaustion value copied before main must already be TERMINAL.
  assert(staticTerminal == TrackerGeneration::terminal());
  assert(staticTerminal != PushStateTrackerTestAccess::generationValue(0));
}
