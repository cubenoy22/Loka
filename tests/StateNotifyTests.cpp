#include "StateNotifyTests.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
#include "loka/core/State.hpp"
#include "core/scheduler/NextTickTracker.hpp"

// ============================================================
// Helpers
// ============================================================

namespace
{

static void increment(void *user)
{
  (*static_cast<int *>(user))++;
}

// --- Call-order tracking ---

struct CallLog
{
  std::vector<int> ids;
};

struct CallLogCtx
{
  CallLog *log;
  int id;
};

static void logId(void *user)
{
  CallLogCtx *ctx = static_cast<CallLogCtx *>(user);
  ctx->log->ids.push_back(ctx->id);
}

// --- Handler-added-during-notification ---

struct AddDuringNotifyCtx
{
  loka::core::EmitterState *state;
  int *counter;
  bool added;
};

static void addHandlerDuringNotify(void *user)
{
  AddDuringNotifyCtx *ctx = static_cast<AddDuringNotifyCtx *>(user);
  if (!ctx->added)
  {
    ctx->added = true;
    ctx->state->bind(&increment, ctx->counter, false);
  }
}

// --- Notify-safety helpers (self-delete / sibling-unbind) ---

struct SafetyCtx
{
  loka::core::EmitterState *emitter;
  loka::core::MutableState<int> *valueState;
  int primaryCalls;
  int siblingCalls;
};

static void deleteEmitter(void *user)
{
  SafetyCtx *ctx = static_cast<SafetyCtx *>(user);
  ++ctx->primaryCalls;
  if (ctx->emitter)
  {
    loka::core::EmitterState *owned = ctx->emitter;
    ctx->emitter = 0;
    delete owned;
  }
}

static void deleteValueState(void *user)
{
  SafetyCtx *ctx = static_cast<SafetyCtx *>(user);
  ++ctx->primaryCalls;
  if (ctx->valueState)
  {
    loka::core::MutableState<int> *owned = ctx->valueState;
    ctx->valueState = 0;
    delete owned;
  }
}

static void countSibling(void *user)
{
  SafetyCtx *ctx = static_cast<SafetyCtx *>(user);
  ++ctx->siblingCalls;
}

static void selfUnbindEmitter(void *user)
{
  SafetyCtx *ctx = static_cast<SafetyCtx *>(user);
  ++ctx->primaryCalls;
  if (ctx->emitter)
    ctx->emitter->unbind(&selfUnbindEmitter, user);
}

static void unbindSiblingFromEmitter(void *user)
{
  SafetyCtx *ctx = static_cast<SafetyCtx *>(user);
  ++ctx->primaryCalls;
  if (ctx->emitter)
    ctx->emitter->unbind(&countSibling, user);
}

static void unbindSiblingFromValueState(void *user)
{
  SafetyCtx *ctx = static_cast<SafetyCtx *>(user);
  ++ctx->primaryCalls;
  if (ctx->valueState)
    ctx->valueState->unbind(&countSibling, user);
}

} // namespace

// ============================================================
// Tests
// ============================================================

void testStateNotify()
{
  printf("\n==== [testStateNotify] start ====\n");

  // --- bind: fires on value change ---
  {
    int count = 0;
    loka::core::MutableState<int> s(0);
    s.bind(&increment, &count, false);
    s.set(1);
    assert(count == 1);
    s.set(2);
    assert(count == 2);
  }

  // --- bind: multiple handlers fire in insertion order (same priority) ---
  {
    CallLog log;
    CallLogCtx a = {&log, 0};
    CallLogCtx b = {&log, 1};
    CallLogCtx c = {&log, 2};
    loka::core::EmitterState s;
    s.bind(&logId, &a, false);
    s.bind(&logId, &b, false);
    s.bind(&logId, &c, false);
    s.emit();
    assert(log.ids.size() == 3);
    assert(log.ids[0] == 0);
    assert(log.ids[1] == 1);
    assert(log.ids[2] == 2);
  }

  // --- bind: higher priority fires before lower priority ---
  {
    CallLog log;
    CallLogCtx lo = {&log, 0};
    CallLogCtx hi = {&log, 1};
    loka::core::EmitterState s;
    s.bind(&logId, &lo, false, false, loka::core::STATE_PRIORITY_NORMAL);
    s.bind(&logId, &hi, false, false, loka::core::STATE_PRIORITY_HIGH);
    s.emit();
    assert(log.ids.size() == 2);
    assert(log.ids[0] == 1); // high fires first
    assert(log.ids[1] == 0);
  }

  // --- unbind: removes handler, no further calls ---
  {
    int count = 0;
    loka::core::EmitterState s;
    s.bind(&increment, &count, false);
    s.unbind(&increment, &count);
    s.emit();
    assert(count == 0);
  }

  // --- callOnce: fires exactly once, then auto-removed ---
  {
    int count = 0;
    loka::core::EmitterState s;
    s.bind(&increment, &count, false, true); // callOnce=true
    s.emit();
    assert(count == 1);
    s.emit();
    assert(count == 1); // not called again
  }

  // --- callImmediately: fires at bind time ---
  {
    int count = 0;
    loka::core::MutableState<int> s(42);
    s.bind(&increment, &count, true); // callImmediately=true (default)
    assert(count == 1);
    s.set(43);
    assert(count == 2);
  }

  // --- callImmediately + callOnce: fires once at bind time, not again ---
  {
    int count = 0;
    loka::core::MutableState<int> s(42);
    s.bind(&increment, &count, true, true);
    assert(count == 1);
    s.set(43);
    assert(count == 1); // callOnce already fired at bind
  }

  // --- MutableState: same value does not trigger notification ---
  {
    int count = 0;
    loka::core::MutableState<int> s(5);
    s.bind(&increment, &count, false);
    s.set(5); // same value
    assert(count == 0);
    s.set(6);
    assert(count == 1);
  }

  // --- MutableState: forceUpdate notifies even on same value ---
  {
    int count = 0;
    loka::core::MutableState<int> s(5);
    s.bind(&increment, &count, false);
    s.set(5, true); // forceUpdate=true
    assert(count == 1);
  }

  // --- deferBind: deferred handler fires after regular handlers ---
  {
    CallLog log;
    CallLogCtx regular = {&log, 0};
    CallLogCtx deferred = {&log, 1};
    loka::core::EmitterState s;
    s.deferBind(&logId, &deferred);
    s.bind(&logId, &regular, false);
    s.emit();
    assert(log.ids.size() == 2);
    assert(log.ids[0] == 0); // regular first
    assert(log.ids[1] == 1); // deferred second
  }

  // --- deferUnbind: removes deferred handler before it fires ---
  {
    int count = 0;
    loka::core::EmitterState s;
    s.deferBind(&increment, &count);
    s.deferUnbind(&increment, &count);
    s.emit();
    assert(count == 0);
  }

  // --- deferUnbind: removes duplicate deferred handlers, not just the first ---
  {
    int count = 0;
    loka::core::EmitterState s;
    s.deferBind(&increment, &count);
    s.deferBind(&increment, &count);
    s.deferUnbind(&increment, &count);
    s.emit();
    assert(count == 0);
  }

  // --- notify: handler added during notification does not fire in that cycle ---
  {
    int lateCount = 0;
    loka::core::EmitterState s;
    AddDuringNotifyCtx ctx = {&s, &lateCount, false};
    s.bind(&addHandlerDuringNotify, &ctx, false);
    s.emit(); // addHandlerDuringNotify runs, registers increment
    assert(lateCount == 0); // increment was added mid-notify, must not fire yet
    s.emit(); // now increment fires
    assert(lateCount == 1);
  }

  // --- safety: callback may delete EmitterState mid-notify ---
  // Sibling handler registered after the deleter must not fire.
  {
    SafetyCtx ctx;
    ctx.emitter = new loka::core::EmitterState();
    ctx.valueState = 0;
    ctx.primaryCalls = 0;
    ctx.siblingCalls = 0;
    ctx.emitter->bind(&deleteEmitter, &ctx, false);
    ctx.emitter->bind(&countSibling, &ctx, false);
    ctx.emitter->emit();
    assert(ctx.emitter == 0);
    assert(ctx.primaryCalls == 1);
    assert(ctx.siblingCalls == 0);
  }

  // --- safety: callback may delete MutableState mid-notify ---
  {
    SafetyCtx ctx;
    ctx.emitter = 0;
    ctx.valueState = new loka::core::MutableState<int>(0);
    ctx.primaryCalls = 0;
    ctx.siblingCalls = 0;
    ctx.valueState->bind(&deleteValueState, &ctx, false);
    ctx.valueState->bind(&countSibling, &ctx, false);
    ctx.valueState->set(1);
    assert(ctx.valueState == 0);
    assert(ctx.primaryCalls == 1);
    assert(ctx.siblingCalls == 0);
  }

  // --- safety: self-unbind fires once, does not fire on subsequent emits ---
  {
    SafetyCtx ctx;
    loka::core::EmitterState s;
    ctx.emitter = &s;
    ctx.valueState = 0;
    ctx.primaryCalls = 0;
    ctx.siblingCalls = 0;
    s.bind(&selfUnbindEmitter, &ctx, false);
    s.bind(&countSibling, &ctx, false);
    s.emit();
    assert(ctx.primaryCalls == 1);
    assert(ctx.siblingCalls == 1);
    s.emit(); // selfUnbind is gone, only sibling fires
    assert(ctx.primaryCalls == 1);
    assert(ctx.siblingCalls == 2);
  }

  // --- safety: sibling unbound mid-notify (EmitterState) does not fire ---
  {
    SafetyCtx ctx;
    loka::core::EmitterState s;
    ctx.emitter = &s;
    ctx.valueState = 0;
    ctx.primaryCalls = 0;
    ctx.siblingCalls = 0;
    s.bind(&unbindSiblingFromEmitter, &ctx, false);
    s.bind(&countSibling, &ctx, false);
    s.emit();
    assert(ctx.primaryCalls == 1);
    assert(ctx.siblingCalls == 0); // was unbound before its turn
    s.emit(); // sibling is gone; only primary fires
    assert(ctx.primaryCalls == 2);
    assert(ctx.siblingCalls == 0);
  }

  // --- safety: sibling unbound mid-notify (MutableState) does not fire ---
  {
    SafetyCtx ctx;
    loka::core::MutableState<int> s(0);
    ctx.emitter = 0;
    ctx.valueState = &s;
    ctx.primaryCalls = 0;
    ctx.siblingCalls = 0;
    s.bind(&unbindSiblingFromValueState, &ctx, false);
    s.bind(&countSibling, &ctx, false);
    s.set(1);
    assert(ctx.primaryCalls == 1);
    assert(ctx.siblingCalls == 0);
  }

  // --- NextTickTracker: delay accumulation keeps earliest request (min wins) ---
  {
    loka::core::NextTickTracker tracker;
    assert(!tracker.hasPendingRequest());

    tracker.request(1000);
    assert(tracker.hasPendingRequest());
    assert(tracker.pendingDelayMs() == 1000UL);

    tracker.request(0);
    assert(tracker.pendingDelayMs() == 0UL);

    tracker.request(250);
    assert(tracker.pendingDelayMs() == 0UL);
  }

  printf("==== [testStateNotify] end ====\n");
}
