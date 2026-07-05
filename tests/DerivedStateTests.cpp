#include "DerivedStateTests.hpp"
#include <cassert>
#include <cstdio>
#include "core/State.hpp"
#include "core/StateTracker.hpp"

// First unit tests for loka::core::DerivedState (previously zero coverage;
// see issue #47). These pin the *current* core-layer contract so that the
// derived-state propagation work tracked in #46 (F3) can change behavior
// deliberately instead of silently.

// ============================================================
// Helpers
// ============================================================

namespace
{

  static void increment(void *user)
  {
    (*static_cast<int *>(user))++;
  }

  static int g_evalFnDestroyed = 0;

  struct SumEval : public loka::core::DerivedState<int>::EvalFn
  {
    loka::core::State<int> *a;
    loka::core::State<int> *b;
    SumEval(loka::core::State<int> *aState, loka::core::State<int> *bState)
        : a(aState),
          b(bState)
    {
    }
    virtual ~SumEval()
    {
      ++g_evalFnDestroyed;
    }
    virtual int operator()()
    {
      return (a ? a->get() : 0) + (b ? b->get() : 0);
    }
  };

  struct DoubleEval : public loka::core::DerivedState<int>::EvalFn
  {
    loka::core::State<int> *src;
    DoubleEval(loka::core::State<int> *srcState)
        : src(srcState)
    {
    }
    virtual ~DoubleEval()
    {
      ++g_evalFnDestroyed;
    }
    virtual int operator()()
    {
      return src ? src->get() * 2 : 0;
    }
  };

} // namespace

// ============================================================
// Tests
// ============================================================

void testDerivedStateCore()
{
  printf("\n==== [testDerivedStateCore] start ====\n");

  // --- construction evaluates the EvalFn immediately ---
  {
    loka::core::MutableState<int> a(1);
    loka::core::MutableState<int> b(2);
    loka::core::DerivedState<int> sum(&a, &b, new SumEval(&a, &b));
    assert(sum.get() == 3);
  }

  // --- ~DerivedState deletes its EvalFn exactly once ---
  {
    g_evalFnDestroyed = 0;
    loka::core::MutableState<int> a(1);
    loka::core::DerivedState<int> *twice = new loka::core::DerivedState<int>(&a, new DoubleEval(&a));
    assert(twice->get() == 2);
    delete twice;
    assert(g_evalFnDestroyed == 1);
  }

  // --- dependency update inside a tracker transaction recomputes + notifies once ---
  {
    loka::core::MutableState<int> a(1);
    loka::core::MutableState<int> b(2);
    loka::core::DerivedState<int> sum(&a, &b, new SumEval(&a, &b));
    loka::core::PushStateTracker tracker;
    tracker.addState(&a);
    tracker.addState(&b);
    tracker.addState(&sum);
    int notifications = 0;
    sum.bind(&increment, &notifications, false);

    tracker.begin();
    a.set(10);
    bool settled = tracker.end();
    assert(settled);
    assert(sum.get() == 12);
    assert(notifications == 1);

    // Updating both dependencies in one transaction recomputes/notifies once.
    tracker.begin();
    a.set(20);
    b.set(30);
    settled = tracker.end();
    assert(settled);
    assert(sum.get() == 50);
    assert(notifications == 2);
  }

  // --- same-value dependency write: recompute yields no change, no notification ---
  {
    loka::core::MutableState<int> a(5);
    loka::core::MutableState<int> b(0);
    loka::core::DerivedState<int> sum(&a, &b, new SumEval(&a, &b));
    loka::core::PushStateTracker tracker;
    tracker.addState(&a);
    tracker.addState(&b);
    tracker.addState(&sum);
    int notifications = 0;
    sum.bind(&increment, &notifications, false);

    tracker.begin();
    a.set(5); // unchanged value still enters the transaction
    bool settled = tracker.end();
    assert(settled);
    assert(sum.get() == 5);
    assert(notifications == 0);
  }

  // --- chained derived states settle within one transaction ---
  {
    loka::core::MutableState<int> a(1);
    loka::core::DerivedState<int> twice(&a, new DoubleEval(&a));
    loka::core::DerivedState<int> fourTimes(&twice, new DoubleEval(&twice));
    loka::core::PushStateTracker tracker;
    tracker.addState(&a);
    tracker.addState(&twice);
    tracker.addState(&fourTimes);
    int twiceNotifications = 0;
    int fourTimesNotifications = 0;
    twice.bind(&increment, &twiceNotifications, false);
    fourTimes.bind(&increment, &fourTimesNotifications, false);

    tracker.begin();
    a.set(3);
    bool settled = tracker.end();
    assert(settled);
    assert(twice.get() == 6);
    assert(fourTimes.get() == 12);
    // Each derived state notifies exactly once even though the second-level
    // state is visited before its upstream in the first commit pass.
    assert(twiceNotifications == 1);
    assert(fourTimesNotifications == 1);
  }

  // --- without a tracker, a dependency write does NOT recompute ---
  // Documents the current core-layer contract: DerivedState only recomputes
  // through a tracker transaction. The reactive StateStream layer installs its
  // own recompute bindings separately (audit M4 / #46 F3 track any change).
  {
    loka::core::MutableState<int> a(1);
    loka::core::DerivedState<int> twice(&a, new DoubleEval(&a));
    assert(twice.get() == 2);
    a.set(5); // no tracker transaction
    assert(twice.get() == 2); // stale by design at the core layer
  }

  // --- owner cleanup: removeState scrubs dependency edges before destroy ---
  {
    loka::core::MutableState<int> a(1);
    loka::core::PushStateTracker tracker;
    tracker.addState(&a);
    loka::core::DerivedState<int> *twice = new loka::core::DerivedState<int>(&a, new DoubleEval(&a));
    tracker.addState(twice);

    tracker.begin();
    a.set(2);
    bool settled = tracker.end();
    assert(settled);
    assert(twice->get() == 4);

    tracker.removeState(twice);
    delete twice;

    // A later transaction must not touch the destroyed derived state
    // (dependency edges were scrubbed by removeState; ASan verifies).
    tracker.begin();
    a.set(3);
    settled = tracker.end();
    assert(settled);
  }

  printf("==== [testDerivedStateCore] end ====\n");
}
