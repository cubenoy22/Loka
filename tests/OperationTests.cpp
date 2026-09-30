#include "OperationTests.hpp"
#include "core/Operation.hpp"
#include "core/State.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "testing/core/StateTrackerTestAccess.hpp"
#include "support/TestVerify.hpp"
#include <cstdio>
#include <vector>
#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace
{
  using namespace loka::core;
  typedef testing::PushStateTrackerTestAccess Access;

  struct Ledger
  {
    MutableState<int> source;
    PushStateTracker tracker;
    int invalidations;
    Ledger() : source(0), tracker(), invalidations(0)
    {
      this->tracker.addState(&this->source);
      this->tracker.setInvalidateCallback(&Ledger::invalidate, this);
    }
    static void invalidate(void *data)
    {
      ++static_cast<Ledger *>(data)->invalidations;
    }
  };


  // Records the commit snapshot each time a ledger invalidates, so a cross-round
  // pin can see whether a later round's snapshot still carries the earlier one.
  struct SnapshotTrace
  {
    Ledger *ledger;
    std::vector<std::vector<StateBase *> > snapshots;
    static void invoke(void *data)
    {
      SnapshotTrace &trace = *static_cast<SnapshotTrace *>(data);
      ++trace.ledger->invalidations;
      trace.snapshots.push_back(trace.ledger->tracker.committedDirtyStates());
    }
  };

  struct Increment : DerivedState<int>::EvalFn
  {
    State<int> *source;
    explicit Increment(State<int> *s) : source(s) {}
    int operator()() { return this->source->get() + 1; }
  };

  void verifyClosed(const Ledger &ledger)
  {
    assert(ledger.tracker.phase() == TRACKER_IDLE);
    assert(ledger.source.trackerOwner() == 0);
    assert(Access::depth(ledger.tracker) == 0);
    (void)ledger;
  }

  struct Write
  {
    Ledger *from;
    MutableState<int> *to;
    int writes;
    static void invoke(void *data)
    {
      Write &write = *static_cast<Write *>(data);
      ++write.from->invalidations;
      if (write.writes > 0)
      {
        --write.writes;
        if (write.to != &write.from->source)
          assert(write.to->trackerOwner()->phase() == TRACKER_PRECOMMIT);
        write.to->set(write.to->get() + 1);
      }
    }
  };

  struct OpenDuringRound
  {
    Operation *operation;
    Ledger *opened;
    std::vector<int> *trace;
    static void invoke(void *data)
    {
      OpenDuringRound &probe = *static_cast<OpenDuringRound *>(data);
      probe.trace->push_back(1);
      LOKA_VERIFY(probe.operation->open(&probe.opened->tracker) == OPEN_OK);
      probe.opened->source.set(1);
    }
    static void record(void *data)
    {
      static_cast<std::vector<int> *>(data)->push_back(2);
    }
  };

  struct DeferredLoop
  {
    PushStateTracker *tracker;
    int calls;
    int cleanupCalls;
    static void invoke(void *data)
    {
      DeferredLoop &loop = *static_cast<DeferredLoop *>(data);
      ++loop.calls;
      if (loop.tracker->phase() == TRACKER_IDLE)
        ++loop.cleanupCalls;
      Access::defer(*loop.tracker, &DeferredLoop::invoke, &loop);
    }
  };

  struct CleanupWrite
  {
    Ledger *from;
    Ledger *to;
    int calls;
    static void invoke(void *data)
    {
      CleanupWrite &probe = *static_cast<CleanupWrite *>(data);
      assert(probe.from->tracker.phase() == TRACKER_IDLE);
      assert(probe.to->tracker.phase() == TRACKER_IDLE);
      assert(probe.from->source.trackerOwner() == 0);
      assert(probe.to->source.trackerOwner() == 0);
      ++probe.calls;
      // Cleanup runs outside every ledger's routes: this assignment changes
      // the value but deliberately does not create another transaction.
      probe.to->source.set(9);
    }
  };

  struct Rising : DerivedState<int>::EvalFn
  {
    int value;
    Rising() : value(0) {}
    int operator()() { return ++this->value; }
  };

  struct NonPushTracker : StateTracker
  {
    int begins;
    int ends;
    NonPushTracker() : begins(0), ends(0) {}
    void begin() { ++this->begins; }
    bool end() { ++this->ends; return true; }
    void defer(TrackerDeferKey, void (*)(void *), void *) {}
    void markDirty(StateBase *) {}
    void registerDependency(StateBase *, StateBase *) {}
    TrackerPhase phase() const { return TRACKER_IDLE; }
  };

  void count(void *data) { ++*static_cast<int *>(data); }

#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
  void expectAssert(void (*probe)())
  {
    const pid_t child = fork();
    LOKA_VERIFY(child >= 0);
    if (child == 0)
    {
      probe();
      _exit(0);
    }
    int status = 0;
    LOKA_VERIFY(waitpid(child, &status, 0) == child);
    LOKA_VERIFY(WIFSIGNALED(status));
    LOKA_VERIFY(WTERMSIG(status) == SIGABRT);
  }

  void destroyOpenLedger()
  {
    PushStateTracker *tracker = new PushStateTracker;
    Operation operation;
    LOKA_VERIFY(operation.open(tracker) == OPEN_OK);
    delete tracker;
  }

  void nestClocks()
  {
    Operation first;
    Operation second;
  }
  void closeTwice()
  {
    Operation operation;
    operation.close();
    operation.close();
  }
  void openAfterClose()
  {
    Ledger ledger;
    Operation operation;
    operation.close();
    operation.open(&ledger.tracker);
    _exit(0); // Do not let the destructor's separate wall mask this probe.
  }
  void closeWithNestedGuard()
  {
    Ledger ledger;
    Operation operation;
    LOKA_VERIFY(operation.open(&ledger.tracker) == OPEN_OK);
    StateTrackerGuard guard(&ledger.tracker);
    operation.close();
  }

#ifdef LOKA_LIFECYCLE_AUDIT
  void collideBegin()
  {
    MutableState<int> state(0);
    PushStateTracker a, b;
    a.addState(&state);
    b.addState(&state);
    a.begin();
    b.begin();
  }
  void collideAdd()
  {
    MutableState<int> state(0);
    PushStateTracker a, b;
    a.addState(&state);
    a.begin();
    b.begin();
    b.addState(&state);
  }
  void collideUnchecked()
  {
    MutableState<int> state(0);
    PushStateTracker a, b;
    a.addStateUnchecked(&state);
    a.begin();
    b.begin();
    b.addStateUnchecked(&state);
  }
#endif
#endif
}

void testOperationLedgerOpenedDuringRoundWaitsForNextRound()
{
  Ledger a, c;
  std::vector<int> trace;
  Operation operation;
  OpenDuringRound probe = { &operation, &c, &trace };
  a.tracker.setInvalidateCallback(&OpenDuringRound::invoke, &probe);
  c.tracker.setInvalidateCallback(&OpenDuringRound::record, &trace);
  LOKA_VERIFY(operation.open(&a.tracker) == OPEN_OK);
  a.source.set(1);
  const OperationOutcome outcome = operation.close();
  LOKA_VERIFY(outcome.status == OPERATION_SETTLED);
  LOKA_VERIFY(outcome.rounds == 2);
  LOKA_VERIFY(trace.size() == 2 && trace[0] == 1 && trace[1] == 2);
  verifyClosed(a);
  verifyClosed(c);
}

void testOperationWriteToVisitedLedgerSettlesNextRound()
{
  Ledger a, b;
  DerivedState<int> derived(&b.source, new Increment(&b.source));
  b.tracker.addState(&derived);
  Write write = { &a, &b.source, 1 };
  a.tracker.setInvalidateCallback(&Write::invoke, &write);
  Operation operation;
  LOKA_VERIFY(operation.open(&b.tracker) == OPEN_OK);
  LOKA_VERIFY(operation.open(&a.tracker) == OPEN_OK);
  b.source.set(1);
  a.source.set(1);
  const OperationOutcome outcome = operation.close();
  LOKA_VERIFY(outcome.status == OPERATION_SETTLED && outcome.rounds == 2);
  assert(derived.get() == 3);
  assert(b.invalidations == 2);
  assert(b.tracker.transactionDirty());
  assert(b.tracker.peekDirty());
  b.tracker.removeState(&derived);
}

void testOperationSecondRoundSnapshotHoldsOnlyNewIdentities()
{
  // #1062 bot P2: a write into an already-stepped ledger lands in current, so the
  // next step must not keep the previous round's commit snapshot under it.
  Ledger a, b;
  MutableState<int> second(0);
  b.tracker.addState(&second);
  SnapshotTrace trace = { &b, std::vector<std::vector<StateBase *> >() };
  b.tracker.setInvalidateCallback(&SnapshotTrace::invoke, &trace);
  Write write = { &a, &second, 1 };
  a.tracker.setInvalidateCallback(&Write::invoke, &write);
  Operation operation;
  LOKA_VERIFY(operation.open(&b.tracker) == OPEN_OK);
  LOKA_VERIFY(operation.open(&a.tracker) == OPEN_OK);
  b.source.set(1);
  a.source.set(1);
  const OperationOutcome outcome = operation.close();
  LOKA_VERIFY(outcome.status == OPERATION_SETTLED && outcome.rounds == 2);
  LOKA_VERIFY(trace.snapshots.size() == 2);
  LOKA_VERIFY(trace.snapshots[0].size() == 1 && trace.snapshots[0][0] == &b.source);
  LOKA_VERIFY(trace.snapshots[1].size() == 1 && trace.snapshots[1][0] == &second);
  b.tracker.removeState(&second);
}

void testOperationWriteToUnvisitedLedgerSettlesSameRound()
{
  Ledger a, b;
  DerivedState<int> derived(&b.source, new Increment(&b.source));
  b.tracker.addState(&derived);
  Write write = { &a, &b.source, 1 };
  a.tracker.setInvalidateCallback(&Write::invoke, &write);
  Operation operation;
  LOKA_VERIFY(operation.open(&a.tracker) == OPEN_OK);
  LOKA_VERIFY(operation.open(&b.tracker) == OPEN_OK);
  a.source.set(1);
  const OperationOutcome outcome = operation.close();
  LOKA_VERIFY(outcome.status == OPERATION_SETTLED && outcome.rounds == 1);
  assert(derived.get() == 2);
  assert(b.invalidations == 1);
  b.tracker.removeState(&derived);
}

void testOperationCleanLedgerClosesIdleWithoutInvalidate()
{
  Ledger ledger;
  Operation operation;
  LOKA_VERIFY(operation.open(&ledger.tracker) == OPEN_OK);
  const OperationOutcome outcome = operation.close();
  LOKA_VERIFY(outcome.status == OPERATION_SETTLED && outcome.rounds == 0);
  verifyClosed(ledger);
  assert(ledger.invalidations == 0);
  assert(!ledger.tracker.transactionDirty());
}

void testOperationOneWriteSettlesInOneRound()
{
  Ledger ledger;
  Operation operation;
  LOKA_VERIFY(operation.open(&ledger.tracker) == OPEN_OK);
  ledger.source.set(1);
  const OperationOutcome outcome = operation.close();
  LOKA_VERIFY(outcome.status == OPERATION_SETTLED && outcome.rounds == 1);
  verifyClosed(ledger);
  assert(ledger.invalidations == 1);
  assert(ledger.tracker.committedDirtyStates().size() == 1);
}

void testOperationChainLimitClosesAllLedgers()
{
  Ledger a, b;
  Write write = { &a, &a.source, 1001 };
  a.tracker.setInvalidateCallback(&Write::invoke, &write);
  Operation operation;
  LOKA_VERIFY(operation.open(&a.tracker) == OPEN_OK);
  LOKA_VERIFY(operation.open(&b.tracker) == OPEN_OK);
  a.source.set(1);
  const OperationOutcome outcome = operation.close();
  LOKA_VERIFY(outcome.status == OPERATION_REFUSED_CHAIN_LIMIT);
  LOKA_VERIFY(outcome.rounds == 1000);
  assert(a.invalidations == 1000 && b.invalidations == 0);
  assert(a.source.get() == 1001);
  verifyClosed(a);
  verifyClosed(b);
}

void testOperationStateBudgetClosesAllLedgers()
{
  Ledger a, b;
  DerivedState<int> cycle(&a.source, new Rising);
  a.tracker.addState(&cycle);
  a.tracker.registerDependency(&cycle, &cycle);
  Operation operation(OperationBudget(10, 3));
  LOKA_VERIFY(operation.open(&a.tracker) == OPEN_OK);
  LOKA_VERIFY(operation.open(&b.tracker) == OPEN_OK);
  a.source.set(1);
  const OperationOutcome outcome = operation.close();
  LOKA_VERIFY(outcome.status == OPERATION_REFUSED_STATE_BUDGET);
  LOKA_VERIFY(outcome.rounds == 1);
  verifyClosed(a);
  verifyClosed(b);
  assert(cycle.trackerOwner() == 0);
  assert(a.invalidations == 0);
  a.tracker.removeState(&cycle);
}

void testOperationSettlesOnLastPermittedRound()
{
  Ledger ledger;
  Write write = { &ledger, &ledger.source, 2 };
  ledger.tracker.setInvalidateCallback(&Write::invoke, &write);
  Operation operation(OperationBudget(3));
  LOKA_VERIFY(operation.open(&ledger.tracker) == OPEN_OK);
  ledger.source.set(1);
  const OperationOutcome outcome = operation.close();
  LOKA_VERIFY(outcome.status == OPERATION_SETTLED && outcome.rounds == 3);
  assert(ledger.invalidations == 3);
}

void testOperationCloseDrainIsBounded()
{
  Ledger a, b;
  DeferredLoop loop = { &a.tracker, 0, 0 };
  Operation operation(OperationBudget(3));
  LOKA_VERIFY(operation.open(&a.tracker) == OPEN_OK);
  LOKA_VERIFY(operation.open(&b.tracker) == OPEN_OK);
  Access::defer(a.tracker, &DeferredLoop::invoke, &loop);
  const OperationOutcome outcome = operation.close();
  LOKA_VERIFY(outcome.status == OPERATION_REFUSED_CHAIN_LIMIT && outcome.rounds == 3);
  assert(loop.calls == 4 && loop.cleanupCalls == 1);
  assert(a.invalidations == 0);
  assert(Access::currentDeferredCount(a.tracker) == 0);
  assert(Access::nextDeferredCount(a.tracker) == 0);
  verifyClosed(a);
  verifyClosed(b);
}

void testOperationWriteDuringCloseIsUnmarked()
{
  Ledger a, b;
  CleanupWrite probe = { &a, &b, 0 };
  Operation operation(OperationBudget(0));
  LOKA_VERIFY(operation.open(&a.tracker) == OPEN_OK);
  LOKA_VERIFY(operation.open(&b.tracker) == OPEN_OK);
  Access::defer(a.tracker, &CleanupWrite::invoke, &probe);
  const OperationOutcome outcome = operation.close();
  LOKA_VERIFY(outcome.status == OPERATION_REFUSED_CHAIN_LIMIT && outcome.rounds == 0);
  assert(probe.calls == 1);
  assert(b.source.get() == 9);
  assert(!b.tracker.transactionDirty());
  assert(!b.tracker.peekDirty());
  assert(Access::currentDirtyCount(b.tracker) == 0);
  assert(Access::nextDirtyCount(b.tracker) == 0);
  assert(a.invalidations == 0 && b.invalidations == 0);
  verifyClosed(a);
  verifyClosed(b);
}

void testOperationRejectsLedgerDestructionWhileOpen()
{
#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
  expectAssert(&destroyOpenLedger);
#else
  std::printf("[skip] Operation destruction death pin requires Linux debug without ASan.\n");
#endif
}

void testStateTrackerNestedGuardInsideOperationDoesNotDrive()
{
  Ledger ledger;
  int guardInvalidations = 0;
  Operation operation;
  LOKA_VERIFY(operation.open(&ledger.tracker) == OPEN_OK);
  {
    StateTrackerGuard guard(&ledger.tracker, &count, &guardInvalidations);
    ledger.source.set(1);
  }
  assert(Access::depth(ledger.tracker) == 1);
  assert(ledger.tracker.phase() == TRACKER_PRECOMMIT);
  assert(ledger.invalidations == 0 && guardInvalidations == 0);
  const OperationOutcome outcome = operation.close();
  LOKA_VERIFY(outcome.status == OPERATION_SETTLED && outcome.rounds == 1);
  assert(ledger.invalidations == 1 && guardInvalidations == 0);
  verifyClosed(ledger);
}

void testOperationAuditRejectsRouteInstallOverAnotherLedgerBegin()
{
#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG) && defined(LOKA_LIFECYCLE_AUDIT)
  expectAssert(&collideBegin);
#else
  std::printf("[skip] Route begin death pin requires Linux debug audit without ASan.\n");
#endif
}
void testOperationAuditRejectsRouteInstallOverAnotherLedgerAddState()
{
#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG) && defined(LOKA_LIFECYCLE_AUDIT)
  expectAssert(&collideAdd);
#else
  std::printf("[skip] Route addState death pin requires Linux debug audit without ASan.\n");
#endif
}
void testOperationAuditRejectsRouteInstallOverAnotherLedgerAddStateUnchecked()
{
#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG) && defined(LOKA_LIFECYCLE_AUDIT)
  expectAssert(&collideUnchecked);
#else
  std::printf("[skip] Route unchecked death pin requires Linux debug audit without ASan.\n");
#endif
}

void testOperationRefusesNonPushAndBusyLedgers()
{
  NonPushTracker mock;
  Ledger busy, available;
  StateTrackerGuard guard(&busy.tracker);
  Operation operation;
  LOKA_VERIFY(operation.open(&mock) == OPEN_REFUSED_NOT_PUSH);
  LOKA_VERIFY(operation.open(&busy.tracker) == OPEN_REFUSED_BUSY);
  LOKA_VERIFY(operation.open(&available.tracker) == OPEN_OK);
  LOKA_VERIFY(operation.open(&available.tracker) == OPEN_ALREADY_OPEN);
  assert(Access::depth(available.tracker) == 1);
  assert(mock.begins == 0 && mock.ends == 0);
  const OperationOutcome outcome = operation.close();
  LOKA_VERIFY(outcome.status == OPERATION_SETTLED && outcome.rounds == 0);
  assert(busy.tracker.phase() == TRACKER_PRECOMMIT);
  assert(Access::depth(busy.tracker) == 1);
}

namespace
{
  struct RecordDerived
  {
    State<int> *derived;
    int value;
    static void invoke(void *data)
    {
      RecordDerived &probe = *static_cast<RecordDerived *>(data);
      probe.value = probe.derived->get();
    }
  };
}

void testOperationScheduleOrderIsObservable()
{
  Ledger a, b;
  DerivedState<int> derived(&a.source, new Increment(&a.source));
  a.tracker.addState(&derived);
  Write write = { &a, &a.source, 1 };
  RecordDerived record = { &derived, 0 };
  a.tracker.setInvalidateCallback(&Write::invoke, &write);
  b.tracker.setInvalidateCallback(&RecordDerived::invoke, &record);
  Operation operation;
  LOKA_VERIFY(operation.open(&a.tracker) == OPEN_OK);
  LOKA_VERIFY(operation.open(&b.tracker) == OPEN_OK);
  a.source.set(1);
  b.source.set(1);
  const OperationOutcome outcome = operation.close();
  LOKA_VERIFY(outcome.status == OPERATION_SETTLED && outcome.rounds == 2);
  // A depth-first driver would settle A's second commit before B and record 3.
  assert(record.value == 2);
  assert(derived.get() == 3);
  a.tracker.removeState(&derived);
}

void testOperationStateBudgetIsSharedAcrossLedgers()
{
  Ledger a, b;
  Operation operation(OperationBudget(10, 1));
  LOKA_VERIFY(operation.open(&a.tracker) == OPEN_OK);
  LOKA_VERIFY(operation.open(&b.tracker) == OPEN_OK);
  a.source.set(1);
  b.source.set(1);
  const OperationOutcome outcome = operation.close();
  LOKA_VERIFY(outcome.status == OPERATION_REFUSED_STATE_BUDGET && outcome.rounds == 1);
  assert(a.invalidations == 1 && b.invalidations == 0);
  verifyClosed(a);
  verifyClosed(b);
}

void testOperationStateRefusalCleanupUsesRemainingRounds()
{
  Ledger ledger;
  DeferredLoop loop = { &ledger.tracker, 0, 0 };
  Operation operation(OperationBudget(3, 0));
  LOKA_VERIFY(operation.open(&ledger.tracker) == OPEN_OK);
  ledger.source.set(1);
  Access::defer(ledger.tracker, &DeferredLoop::invoke, &loop);
  const OperationOutcome outcome = operation.close();
  LOKA_VERIFY(outcome.status == OPERATION_REFUSED_STATE_BUDGET && outcome.rounds == 1);
  assert(loop.calls == 3 && loop.cleanupCalls == 3);
  assert(Access::currentDeferredCount(ledger.tracker) == 0);
  verifyClosed(ledger);
}

void testOperationDestructorClosesClock()
{
  Ledger ledger;
  {
    Operation operation;
    LOKA_VERIFY(operation.open(&ledger.tracker) == OPEN_OK);
    ledger.source.set(1);
  }
  verifyClosed(ledger);
  assert(ledger.invalidations == 1);
  // A new clock can follow the destroyed clock in the same main-thread stack.
  Operation next;
  LOKA_VERIFY(next.open(&ledger.tracker) == OPEN_OK);
  const OperationOutcome outcome = next.close();
  LOKA_VERIFY(outcome.status == OPERATION_SETTLED && outcome.rounds == 0);
}

void testOperationReleaseWithdrawalAndRouteReplacement()
{
#if defined(NDEBUG) && !defined(LOKA_LIFECYCLE_AUDIT)
  // The release fallback unlinks without visiting rows whose States died
  // with their window. Exercise middle, tail, head, and sole membership.
  Operation operation;
  PushStateTracker *a = new PushStateTracker;
  PushStateTracker *b = new PushStateTracker;
  PushStateTracker *c = new PushStateTracker;
  MutableState<int> *state = new MutableState<int>(0);
  b->addState(state);
  LOKA_VERIFY(operation.open(a) == OPEN_OK);
  LOKA_VERIFY(operation.open(b) == OPEN_OK);
  LOKA_VERIFY(operation.open(c) == OPEN_OK);
  delete state;
  delete b;
  delete c;
  delete a;
  Ledger survivor;
  LOKA_VERIFY(operation.open(&survivor.tracker) == OPEN_OK);
  survivor.source.set(1);
  const OperationOutcome outcome = operation.close();
  LOKA_VERIFY(outcome.status == OPERATION_SETTLED && outcome.rounds == 1);
  LOKA_VERIFY(survivor.invalidations == 1);

  MutableState<int> shared(0);
  PushStateTracker first, second;
  first.addState(&shared);
  second.addState(&shared);
  first.begin();
  second.begin();
  StateTracker *route = shared.trackerOwner();
  LOKA_VERIFY(route == &second);
  LOKA_VERIFY(first.end());
  route = shared.trackerOwner();
  LOKA_VERIFY(route == &second);
  shared.set(1);
  LOKA_VERIFY(second.end());
  const bool dirty = second.transactionDirty();
  LOKA_VERIFY(dirty);
  route = shared.trackerOwner();
  LOKA_VERIFY(route == 0);
#else
  std::printf("[skip] Release withdrawal/route replacement requires NDEBUG without audit.\n");
#endif
}

void testOperationClockContracts()
{
#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
  expectAssert(&nestClocks);
  expectAssert(&closeTwice);
  expectAssert(&openAfterClose);
  expectAssert(&closeWithNestedGuard);
#else
  std::printf("[skip] Operation clock death pins require Linux debug without ASan.\n");
#endif
}
