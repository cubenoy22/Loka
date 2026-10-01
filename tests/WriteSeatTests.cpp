#include "platform/null/NullInputDoor.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "platform/null/NullWindow.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "support/WindowAdmissionTestApp.hpp"
#include <cstdio>


#include "WriteSeatTests.hpp"
#include "support/TestVerify.hpp"
#include "app/scene/state/NodeState.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/nodes/controls/PopupMenu.hpp"
#include "core/State.hpp"
#include "core/StateTracker.hpp"
#include "core/util/StateTrackerGuard.hpp"

using namespace loka::core;
using namespace loka::app;
using namespace loka::app::scene;
namespace { struct Count { int calls; Count() : calls(0) {} static void hit(void *p) { ++static_cast<Count *>(p)->calls; } }; }

void testWriteSeatSettlesIdleOwnerTracker()
{
  MutableState<int> value(0); PushStateTracker tracker; tracker.addState(&value);
  NodeState<int> state(&value, &tracker); Count count; value.bind(&Count::hit, &count, false);
  state.writeSeat().set(2, true);
  LOKA_VERIFY(value.get() == 2 && count.calls == 1 && tracker.phase() == TRACKER_IDLE);
}
void testWriteSeatJoinsOpenTracker()
{
  MutableState<int> value(0); PushStateTracker tracker; tracker.addState(&value);
  NodeState<int> state(&value, &tracker); Count count; value.bind(&Count::hit, &count, false);
  { StateTrackerGuard guard(&tracker); state.writeSeat().set(2, true); LOKA_VERIFY(count.calls == 1); }
  LOKA_VERIFY(count.calls == 1 && tracker.phase() == TRACKER_IDLE);
}
void testWriteSeatRawAndInvalid()
{
  MutableState<int> value(0); Count count; value.bind(&Count::hit, &count, false);
  WriteSeat<int>(&value).set(1, true); LOKA_VERIFY(value.get() == 1 && count.calls == 1);
  WriteSeat<int> invalid; invalid.set(2, true); LOKA_VERIFY(value.get() == 1);
}
void testWriteSeatPropsIdentity()
{
  MutableState<int> value(0); PushStateTracker a; PushStateTracker b; NodeState<int> left(&value, &a), right(&value, &b);
  PopupMenuProps one; one.selectedIndex(left); PopupMenuProps two; two.selectedIndex(right);
  LOKA_VERIFY(!(one < two) && !(two < one));
}

namespace
{
  struct Input977Ledger
  {
    unsigned sequence, destroyed, returned, emitted, applies;
    Input977Ledger() : sequence(0), destroyed(0), returned(0), emitted(0), applies(0) {}
  };
  Input977Ledger *input977Ledger = 0;
  bool input977ParentShown = false;
  class Input977Root;
  class Input977Child;
  Input977Root *input977Root = 0;
  Input977Child *input977Child = 0;

  class Input977Context : public NullScrollBarContext
  {
  public:
    Input977Context(ScrollBarNode &node, NullScenePlatformController &platform)
        : NullScrollBarContext(&node, &platform) {}
    virtual ~Input977Context()
    {
      input977Ledger->destroyed = ++input977Ledger->sequence;
    }
  };
  class Input977Root : public BoundaryNodeFor<Input977Root>
  {
  public:
    NodeState<bool> shown;
    explicit Input977Root(const BoundaryPropsFor<Input977Root> &p) : BoundaryNodeFor<Input977Root>(p)
    {
      input977Root = this;
      this->state(this->shown, true);
    }
    virtual void composeNode(NodeComposition &c);
  };
  class Input977Child : public BoundaryNodeFor<Input977Child>
  {
  public:
    NodeState<int> value;
    EmitterState changed;
    NodeState<bool> shown;
    explicit Input977Child(const BoundaryPropsFor<Input977Child> &p) : BoundaryNodeFor<Input977Child>(p)
    {
      input977Child = this;
      this->state(this->value, 0);
      this->state(this->shown, true);
    }
    virtual void composeNode(NodeComposition &c)
    {
      c.declare(Show(*(input977ParentShown ? input977Root->shown.state() : this->shown.state()))
                    .destroyOnDetach() << ScrollBar(this->value).range(0, 10).onChange(&this->changed));
    }
  };
  void Input977Root::composeNode(NodeComposition &c) { c.declare(Boundary<Input977Child>()); }
  ScrollBarNode *input977Find(Node *node)
  {
    if (node->nodeTypeKey() == NodeTypeToken<ScrollBarNode>())
      return static_cast<ScrollBarNode *>(node);
    INestable *children = node->asNestable();
    for (Node *child = children ? children->childrenHead() : 0; child; child = child->nextInComposition)
    {
      ScrollBarNode *found = input977Find(child);
      if (found) return found;
    }
    return 0;
  }
  class Input977Presenter : public NullScenePlatformController
  {
  public:
    virtual bool canSkipGlobalChangeForBoundaryLocalPaint() const { return false; }
    virtual void onChange(Node *, NodeDirtyFlags, bool) { ++input977Ledger->applies; }
  };
  void input977Emitted(void *)
  {
    LOKA_VERIFY(input977Child->value.get() == 1);
    LOKA_VERIFY(input977Ledger->destroyed == 0);
    input977Ledger->emitted = ++input977Ledger->sequence;
  }
  struct Input977Observer
  {
    NodeState<bool> &shown;
    unsigned calls;
    Input977Observer(NodeState<bool> &s) : shown(s), calls(0)
    { input977Child->value.state()->bind(&changed, this, false); }
    ~Input977Observer() { input977Child->value.state()->unbind(&changed, this); }
    static void changed(void *data)
    {
      Input977Observer &self = *static_cast<Input977Observer *>(data);
      ++self.calls;
      const bool values[] = {false, true, false};
      for (unsigned i = 0; i < 3; ++i)
      {
        self.shown.set(values[i]);
      }
    }
  };
}

void testNullInputDoorLifetime()
{
  for (unsigned arrangement = 0; arrangement < 2; ++arrangement)
  {
    Input977Ledger ledger;
    input977Ledger = &ledger;
    input977ParentShown = arrangement != 0;
    NullPlatformContext context;
    Input977Presenter platform;
    WindowProps props;
    props.scene(new Scene(Boundary<Input977Root>()));
    NullWindow window(&context, props, &platform);
    WindowAdmissionTestApp app(window);
    app.flush();
    Scene &scene = *window.scene();
    ScrollBarNode *node = input977Find(loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
    LOKA_VERIFY(node);
    Input977Context *input = new Input977Context(*node, platform);
    node->setContext(input);
    input->readLifecycleFactOnAttach();
    NodeState<bool> &shown = arrangement ? input977Root->shown : input977Child->shown;
    Input977Observer observer(shown);
    input977Child->changed.bind(&input977Emitted, 0, false);
    ledger.applies = 0;
    NullInputDoor::simulateThumbDragTo(*input, 1);
    ledger.returned = ++ledger.sequence;
    std::fprintf(stderr, "[977] arrangement=%u returned=%u destroyed=%u applies=%u\n",
                 arrangement, ledger.returned, ledger.destroyed, ledger.applies);
    LOKA_VERIFY(observer.calls == 1 && input977Child->value.get() == 1);
    LOKA_VERIFY(ledger.destroyed == 0);
    LOKA_VERIFY(ledger.emitted != 0 && ledger.returned > ledger.emitted);
    LOKA_VERIFY(ledger.applies == 0);
    LOKA_VERIFY(!platform.borrowPhase().open());
    // Returning through the door never pumps: even the final false waits.
    LOKA_VERIFY(input977Find(loka::dsl::testing::SceneTestAccess::rootBoundary(scene)) == node);
    app.flush();
    LOKA_VERIFY(ledger.destroyed > ledger.returned && ledger.applies > 0);
    LOKA_VERIFY(!input977Find(loka::dsl::testing::SceneTestAccess::rootBoundary(scene)));
    input977Child->changed.unbind(&input977Emitted, 0);
  }
  input977Ledger = 0;
  input977Root = 0;
  input977Child = 0;
}

#include "testing/core/StateTrackerTestAccess.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/nestable/Box.hpp"

namespace
{
  typedef loka::core::testing::PushStateTrackerTestAccess SeatAccess;
  typedef loka::core::testing::OperationTestAccess SeatClockAccess;
  struct SeatDouble : DerivedState<int>::EvalFn
  {
    State<int> &source;
    explicit SeatDouble(State<int> &value) : source(value) {}
    virtual int operator()() { return this->source.get() * 2; }
  };
  struct SeatLedger
  {
    MutableState<int> source;
    DerivedState<int> derived;
    Count commits, direct;
    PushStateTracker tracker;
    NodeState<int> seat;
    SeatLedger() : source(1), derived(&source, new SeatDouble(source)), seat(&source, &tracker)
    {
      this->tracker.addState(&this->source);
      this->tracker.addState(&this->derived);
      this->tracker.setInvalidateCallback(&Count::hit, &this->commits);
      this->source.bind(&Count::hit, &this->direct, false);
    }
  };
  struct CleanupSeat
  {
    SeatLedger &target;
    unsigned calls;
    static void write(void *data)
    {
      CleanupSeat &self = *static_cast<CleanupSeat *>(data);
      ++self.calls;
      LOKA_VERIFY(self.target.tracker.phase() == TRACKER_IDLE);
      LOKA_VERIFY(!self.target.source.trackerOwner());
      LOKA_VERIFY(Operation::openActive(&self.target.tracker) == OPEN_REFUSED_CLOSING);
      self.target.seat.set(9);
    }
  };
  struct AbstractSeatTracker : StateTracker
  {
    unsigned begins, ends;
    AbstractSeatTracker() : begins(0), ends(0) {}
    virtual void begin() { ++this->begins; }
    virtual bool end() { ++this->ends; return true; }
    virtual void defer(TrackerDeferKey, void (*)(void *), void *) {}
    virtual void markDirty(StateBase *) {}
    virtual void registerDependency(StateBase *, StateBase *) {}
    virtual TrackerPhase phase() const { return TRACKER_IDLE; }
  };
}

void testSeatInsideTurnEnrollsOwnerAndSettlesAtTail()
{
  SeatLedger f;
  LOKA_VERIFY(f.derived.get() == 2);
  Operation turn;
  f.seat.set(2);
  LOKA_VERIFY(f.source.get() == 2 && f.direct.calls == 1 && f.derived.get() == 2);
  LOKA_VERIFY(f.commits.calls == 0 && SeatAccess::depth(f.tracker) == 1);
  LOKA_VERIFY(Operation::openActive(&f.tracker) == OPEN_ALREADY_OPEN);
  f.seat.set(3);
  LOKA_VERIFY(f.source.get() == 3 && f.direct.calls == 2 && f.derived.get() == 2);
  LOKA_VERIFY(f.commits.calls == 0 && SeatAccess::depth(f.tracker) == 1);
  const OperationOutcome settled = turn.settle();
  LOKA_VERIFY(settled.rounds == 1 && settled.status == OPERATION_SETTLED);
  LOKA_VERIFY(f.derived.get() == 6 && f.commits.calls == 1);
  LOKA_VERIFY(SeatAccess::depth(f.tracker) == 1);
  turn.close();
  LOKA_VERIFY(f.tracker.phase() == TRACKER_IDLE && SeatAccess::depth(f.tracker) == 0);
}

void testSeatOutsideClockKeepsLegacyTransaction()
{
  SeatLedger f;
  LOKA_VERIFY(!Operation::hasActive() && f.derived.get() == 2);
  f.seat.set(4);
  LOKA_VERIFY(f.derived.get() == 8 && f.commits.calls == 1 && f.direct.calls == 1);
  LOKA_VERIFY(f.tracker.phase() == TRACKER_IDLE && !f.source.trackerOwner());
}

void testSeatUnderLegacyGuardWritesOnly()
{
  SeatLedger f;
  Operation turn;
  {
    StateTrackerGuard guard(&f.tracker);
    f.seat.set(4);
    LOKA_VERIFY(Operation::openActive(&f.tracker) == OPEN_REFUSED_BUSY);
    LOKA_VERIFY(SeatClockAccess::empty(turn));
    LOKA_VERIFY(f.derived.get() == 2 && f.commits.calls == 0);
    LOKA_VERIFY(SeatAccess::depth(f.tracker) == 1);
  }
  LOKA_VERIFY(f.derived.get() == 8 && f.commits.calls == 1);
  const OperationOutcome closed = turn.close();
  LOKA_VERIFY(closed.rounds == 0 && f.tracker.phase() == TRACKER_IDLE);
}

void testSeatFirstNestedGuardCommitsAtTail()
{
  SeatLedger f;
  Operation turn;
  f.seat.set(2);
  {
    StateTrackerGuard guard(&f.tracker);
    f.seat.set(4);
    LOKA_VERIFY(SeatAccess::depth(f.tracker) == 2);
  }
  LOKA_VERIFY(SeatAccess::depth(f.tracker) == 1 && f.commits.calls == 0 && f.derived.get() == 2);
  turn.close();
  LOKA_VERIFY(f.derived.get() == 8 && f.commits.calls == 1);
}

void testSeatDuringCleanupWritesWithoutRoute()
{
  SeatLedger f;
  Operation turn(OperationBudget(0));
  LOKA_VERIFY(turn.open(&f.tracker) == OPEN_OK);
  CleanupSeat probe = { f, 0 };
  SeatAccess::defer(f.tracker, &CleanupSeat::write, &probe);
  const OperationOutcome closed = turn.close();
  LOKA_VERIFY(closed.status == OPERATION_REFUSED_CHAIN_LIMIT && probe.calls == 1);
  LOKA_VERIFY(f.source.get() == 9 && f.direct.calls == 1 && f.derived.get() == 2);
  LOKA_VERIFY(f.commits.calls == 0 && !f.tracker.transactionDirty());
  LOKA_VERIFY(SeatAccess::currentDirtyCount(f.tracker) == 0 && SeatAccess::nextDirtyCount(f.tracker) == 0);
}

void testSeatAbstractTrackerKeepsLegacyTransaction()
{
  MutableState<int> source(0);
  AbstractSeatTracker tracker;
  NodeState<int> seat(&source, &tracker);
  Operation turn;
  seat.set(2);
  LOKA_VERIFY(source.get() == 2 && tracker.begins == 1 && tracker.ends == 1);
  LOKA_VERIFY(SeatClockAccess::empty(turn));
}

namespace
{
  unsigned seatBranchConstructions = 0;
  unsigned seatOwnersDestroyedInTurn = 0;
  class SeatBranch : public BoundaryNodeFor<SeatBranch>
  {
  public:
    explicit SeatBranch(const BoundaryPropsFor<SeatBranch> &p) : BoundaryNodeFor<SeatBranch>(p)
    { ++seatBranchConstructions; }
    virtual void composeNode(NodeComposition &c) { c.declare(Box()); }
  };
  struct SeatNodeDouble : DerivedState<int>::EvalFn
  {
    const NodeState<int> &source;
    explicit SeatNodeDouble(const NodeState<int> &value) : source(value) {}
    virtual int operator()() { return this->source.get() * 2; }
  };
  struct SeatVisible : DerivedState<bool>::EvalFn
  {
    const NodeState<bool> &first, &second;
    SeatVisible(const NodeState<bool> &a, const NodeState<bool> &b) : first(a), second(b) {}
    virtual bool operator()() { return this->first.get() && this->second.get(); }
  };
  class SeatRoot : public BoundaryNodeFor<SeatRoot>
  {
  public:
    NodeState<bool> shown, allowed;
    DerivedNodeState<bool> visible;
    NodeState<int> width;
    DerivedNodeState<int> doubled;
    explicit SeatRoot(const BoundaryPropsFor<SeatRoot> &p) : BoundaryNodeFor<SeatRoot>(p)
    {
      this->state(this->shown, false);
      this->state(this->allowed, true);
      this->derived(this->visible, this->shown, this->allowed, new SeatVisible(this->shown, this->allowed));
      this->state(this->width, 10);
      this->derived(this->doubled, this->width, new SeatNodeDouble(this->width));
    }
    virtual ~SeatRoot() { if (Operation::hasActive()) ++seatOwnersDestroyedInTurn; }
    virtual void composeNode(NodeComposition &c)
    {
      c.declare(Column() << Box().width(this->doubled.state())
                        << (Show(*this->visible.state()).destroyOnDetach() << Boundary<SeatBranch>()));
    }
    static void toggle(void *data)
    {
      SeatRoot &root = *static_cast<SeatRoot *>(data);
      root.shown.set(true);
      root.allowed.set(false);
    }
  };
  class SeatPresenter : public NullScenePlatformController
  {
  public:
    unsigned applies;
    SeatPresenter() : applies(0) {}
    virtual void beginApplyCycle() { ++this->applies; }
    virtual bool canSkipGlobalChangeForBoundaryLocalPaint() const { return false; }
  };
  struct SeatScene
  {
    NullPlatformContext context;
    SeatPresenter platform;
    NullWindow window;
    WindowAdmissionTestApp app;
    SeatScene() : window(&context, props(), &platform), app(window) { this->app.flush(); }
    static WindowProps props()
    { return WindowProps().scene(new Scene(Boundary<SeatRoot>())); }
    SeatRoot &root()
    { return *static_cast<SeatRoot *>(loka::dsl::testing::SceneTestAccess::rootBoundary(*this->window.scene())); }
  };
}

void testTwoSeatsOneHandlerProjectOnce()
{
  SeatAccess::InvalidationProbe commits;
  SeatScene f;
  commits.install(*f.root().tracker()->asPushTracker());
  const unsigned before = f.platform.applies;
  seatBranchConstructions = 0;
  f.app.operationLoop(&SeatRoot::toggle, &f.root());
  LOKA_VERIFY(f.platform.applies == before + 1 && commits.calls == 1);
  LOKA_VERIFY(seatBranchConstructions == 0 && !f.root().visible.get());
}

void testTwoSeatsWithoutClockProjectTwice()
{
  SeatScene f;
  const unsigned before = f.platform.applies;
  seatBranchConstructions = 0;
  SeatRoot::toggle(&f.root());
  LOKA_VERIFY(f.platform.applies == before + 2);
  LOKA_VERIFY(seatBranchConstructions == 1 && !f.root().visible.get());
}

namespace
{
  struct ThreeSeatOwners
  {
    SeatLedger owners[3];
    Scene *scenes[2];
    static void invalidate(void *data)
    {
      ThreeSeatOwners &self = *static_cast<ThreeSeatOwners *>(data);
      for (unsigned i = 0; i < 2; ++i)
        self.scenes[i]->requestInvalidate();
    }
    static void collect(void *data)
    {
      ThreeSeatOwners &self = *static_cast<ThreeSeatOwners *>(data);
      for (unsigned i = 0; i < 3; ++i)
      {
        self.owners[i].seat.set(3 + i);
        self.owners[i].seat.set(4 + i);
      }
    }
  };
  class ThreeSeatPresenter : public SeatPresenter
  {
  public:
    ThreeSeatOwners *owners;
    ThreeSeatPresenter() : owners(0) {}
    virtual void beginApplyCycle()
    {
      SeatPresenter::beginApplyCycle();
      if (!this->owners) return;
      for (unsigned i = 0; i < 3; ++i)
      {
        LOKA_VERIFY(this->owners->owners[i].derived.get() == static_cast<int>((4 + i) * 2));
        LOKA_VERIFY(this->owners->owners[i].tracker.transactionDirty());
      }
    }
  };
}

void testThreeSeatOwnersTwoScenesSeeEveryCommit()
{
  SeatAccess::InvalidationProbe commits[3];
  ThreeSeatOwners owners;
  NullPlatformContext context;
  ThreeSeatPresenter first, second;
  NullWindow a(&context, SeatScene::props(), &first), b(&context, SeatScene::props(), &second);
  WindowAdmissionTestApp app(a, &b);
  app.flush();
  owners.scenes[0] = a.scene(); owners.scenes[1] = b.scene();
  for (unsigned i = 0; i < 3; ++i)
  {
    owners.owners[i].tracker.setInvalidateCallback(&ThreeSeatOwners::invalidate, &owners);
    commits[i].install(owners.owners[i].tracker);
  }
  first.owners = &owners; second.owners = &owners;
  const unsigned aBefore = first.applies, bBefore = second.applies;
  app.operationLoop(&ThreeSeatOwners::collect, &owners);
  LOKA_VERIFY(first.applies == aBefore + 1 && second.applies == bBefore + 1);
  for (unsigned i = 0; i < 3; ++i) LOKA_VERIFY(commits[i].calls == 1);
  first.owners = 0; second.owners = 0;
}

namespace
{
  void replaceSeatOwner(void *data)
  {
    SeatScene &f = *static_cast<SeatScene *>(data);
    f.root().width.set(30);
    LOKA_VERIFY(f.window.sceneManager()->commitTransaction(0, new Scene(Boundary<SeatRoot>())));
  }
}
void testSeatOwnerDestroyedDuringApplyLeavesClock()
{
  SeatScene f;
  seatOwnersDestroyedInTurn = 0;
  f.app.operationLoop(&replaceSeatOwner, &f);
  LOKA_VERIFY(seatOwnersDestroyedInTurn == 1);
  LOKA_VERIFY(!Operation::hasActive());
  LOKA_VERIFY(f.root().width.get() == 10 && f.root().doubled.get() == 20);
  f.app.operationLoop();
  LOKA_VERIFY(!f.window.scene()->hasPendingInvalidation());
}

#include "dsl/flow/Flow.hpp"
namespace
{
  struct GuardedSeatStep
  {
    typedef int In;
    typedef int Out;
    SeatLedger &owner;
    explicit GuardedSeatStep(SeatLedger &value) : owner(value) {}
    loka::dsl::StepRunStatus run(const int &input, int &out, loka::dsl::FlowError &) const
    {
      StateTrackerGuard guard(&this->owner.tracker);
      this->owner.seat.set(input);
      out = input;
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }
  };
}
void testFlowStepSeatGuardOrders()
{
  for (unsigned seatFirst = 0; seatFirst != 2; ++seatFirst)
  {
    SeatLedger owner;
    int input = 5;
    // Bounded one-shot chain; the owner outlives every step invocation.
    loka::dsl::FlowChain<int, int> flow =
        loka::dsl::Flow() | loka::dsl::Step(1, GuardedSeatStep(owner)).input(&input);
    Operation turn;
    if (seatFirst) owner.seat.set(2);
    LOKA_VERIFY(flow.run());
    LOKA_VERIFY(owner.source.get() == 5);
    LOKA_VERIFY(owner.derived.get() == (seatFirst ? 2 : 10));
    LOKA_VERIFY(owner.commits.calls == (seatFirst ? 0 : 1));
    turn.close();
    LOKA_VERIFY(owner.derived.get() == 10 && owner.commits.calls == 1);
  }
}
