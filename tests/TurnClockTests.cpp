#include "TurnClockTests.hpp"
#include "core/Operation.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "testing/core/StateTrackerTestAccess.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "platform/null/NullWindow.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "support/WindowAdmissionTestApp.hpp"
#include "support/TestVerify.hpp"
#include <cstdio>

namespace
{
  using namespace loka::core;
  using namespace loka::app::scene;
  typedef loka::core::testing::OperationTestAccess ClockAccess;

  struct FlushProbe
  {
    Scene *scene;
    unsigned calls;
    bool flushed;
    bool settling;
    static void invalidate(void *data)
    {
      FlushProbe &probe = *static_cast<FlushProbe *>(data);
      ++probe.calls;
      probe.settling = Operation::isSettling();
      probe.scene->requestInvalidate();
      probe.flushed = probe.scene->flushInvalidation();
    }
  };

  class TurnController : public NullScenePlatformController
  {
  public:
    TurnController() : applies(0), writeTo(0), tracker(0), clockAtApply(0) {}
    virtual void beginApplyCycle()
    {
      ++this->applies;
      this->clockAtApply = ClockAccess::active();
      if (this->writeTo)
      {
        MutableState<int> *target = this->writeTo;
        this->writeTo = 0;
        StateTrackerGuard guard(this->tracker);
        target->set(target->get() + 1);
      }
    }
    unsigned applies;
    MutableState<int> *writeTo;
    PushStateTracker *tracker;
    Operation *clockAtApply;
  };

  struct TurnFixture
  {
    NullPlatformContext context;
    TurnController controller;
    NullWindow window;
    WindowAdmissionTestApp app;
    MutableState<int> value;
    PushStateTracker ledger;
    FlushProbe probe;
    TurnFixture() : window(&context, props(), &controller), app(window), value(0)
    {
      this->app.flush();
      this->probe.scene = this->window.scene();
      this->probe.calls = 0;
      this->probe.flushed = false;
      this->probe.settling = false;
      this->ledger.addState(&this->value);
      this->ledger.setInvalidateCallback(&FlushProbe::invalidate, &this->probe);
    }
    static WindowProps props()
    {
      WindowProps result;
      result.scene(new Scene(new loka::app::BoxDefinition()));
      return result;
    }
  };

  struct BorrowProbe
  {
    TurnController &controller;
    Operation &turn;
    void invoke()
    {
      LOKA_VERIFY(this->controller.borrowPhase().open());
      LOKA_VERIFY(ClockAccess::active() == &this->turn);
      LOKA_VERIFY(!Operation::isSettling());
    }
  };
}

void testSceneFlushIsRefusedWhileClockSettles()
{
  TurnFixture f;
  const unsigned before = f.controller.applies;
  Operation turn;
  LOKA_VERIFY(turn.open(&f.ledger) == OPEN_OK);
  {
    StateTrackerGuard guard(&f.ledger);
    f.value.set(1);
  }
  turn.settle();
  LOKA_VERIFY(f.probe.calls == 1 && f.probe.settling && !f.probe.flushed);
  LOKA_VERIFY(f.window.scene()->hasPendingInvalidation());
  LOKA_VERIFY(f.controller.applies == before);
  f.app.admitAndApplyWindows();
  LOKA_VERIFY(f.controller.applies == before + 1);
  LOKA_VERIFY(!f.window.scene()->hasPendingInvalidation());
  f.app.admitAndApplyWindows();
  LOKA_VERIFY(f.controller.applies == before + 1);
  turn.close();
  f.app.reclaimWindows();
}

void testEmptyTurnKeepsLegacySynchronousFlush()
{
  TurnFixture f;
  const unsigned before = f.controller.applies;
  Operation turn;
  {
    StateTrackerGuard guard(&f.ledger);
    f.value.set(1);
  }
  LOKA_VERIFY(f.probe.calls == 1 && !f.probe.settling && f.probe.flushed);
  LOKA_VERIFY(f.controller.applies == before + 1);
  LOKA_VERIFY(!f.window.scene()->hasPendingInvalidation());
  const OperationOutcome outcome = turn.close();
  LOKA_VERIFY(outcome.status == OPERATION_SETTLED && outcome.rounds == 0);
}

void testApplyTimeWriteSettlesAtCloseAndProjectsNextTurn()
{
  {
    TurnFixture f;
    const unsigned before = f.controller.applies;
    Operation turn;
    LOKA_VERIFY(turn.open(&f.ledger) == OPEN_OK);
    f.controller.writeTo = &f.value;
    f.controller.tracker = &f.ledger;
    f.window.scene()->requestInvalidate();
    turn.settle();
    f.app.admitAndApplyWindows();
    f.app.admitAndApplyWindows();
    LOKA_VERIFY(f.value.get() == 1 && f.probe.calls == 0);
    LOKA_VERIFY(f.controller.applies == before + 1);
    turn.close();
    LOKA_VERIFY(f.probe.calls == 1 && f.probe.settling && !f.probe.flushed);
    LOKA_VERIFY(f.window.scene()->hasPendingInvalidation());
    f.app.reclaimWindows();
    f.app.operationLoop();
    LOKA_VERIFY(f.controller.applies == before + 2);
    LOKA_VERIFY(!f.window.scene()->hasPendingInvalidation());
  }
  {
    TurnFixture f;
    const unsigned before = f.controller.applies;
    f.controller.writeTo = &f.value;
    f.controller.tracker = &f.ledger;
    f.window.scene()->requestInvalidate();
    f.app.admitAndApplyWindows();
    f.app.admitAndApplyWindows();
    f.app.reclaimWindows();
    LOKA_VERIFY(f.value.get() == 1 && f.probe.calls == 1 && !f.probe.settling);
    LOKA_VERIFY(f.controller.applies == before + 2);
    LOKA_VERIFY(!f.window.scene()->hasPendingInvalidation());
  }
  std::printf("B3: clock=apply/close/pending/next-apply legacy=two-applies-in-tail\n");
}

void testCompletionTurnLeavesNoActiveClock()
{
  TurnFixture f;
  f.window.scene()->requestInvalidate();
  f.app.operationLoop();
  LOKA_VERIFY(f.controller.clockAtApply != 0);
  LOKA_VERIFY(!ClockAccess::active());
  LOKA_VERIFY(!Operation::isSettling());
}

void testInputInvocationInsideTurnDoesNotOpenClock()
{
  TurnController controller;
  Operation turn;
  BorrowProbe probe = { controller, turn };
  loka::app::scene::detail::InputInvocation(controller).operator()(probe, &BorrowProbe::invoke);
  LOKA_VERIFY(!controller.borrowPhase().open());
  LOKA_VERIFY(ClockAccess::active() == &turn);
  turn.close();
  LOKA_VERIFY(!ClockAccess::active());
}
