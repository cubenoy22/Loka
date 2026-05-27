# State Update Result Draft

This draft captures the planned direction for explicit State update results,
transaction failure reporting, and Flow integration. It is not a stable API
contract.

## Problem

`MutableState<T>::set()` is intentionally small and convenient. That is still
the right default for ordinary UI updates. However, some update paths need to
know whether an attempted State change was accepted, rejected, discarded, or
made the enclosing StateTracker transaction fail to settle.

Important cases include:

- bidirectional converters;
- sliders and continuous inputs;
- validation and quantized updates;
- owner-side apply steps;
- stale result dropping;
- update-loop detection;
- Flow `onFailure` integration.

Without a first-class result model, applications tend to invent local flags,
version counters, or mutable "currently updating" guards. Those patterns make
feedback loops hard to inspect and hard to test.

## Direction

Keep ordinary `set()` lightweight, and add explicit checked APIs for code paths
that need failure handling.

Possible names:

```cpp
StateSetResult result = state.trySet(value);
StateSetResult result = state.setChecked(value);
```

`trySet` is the preferred working name because it communicates that the update
may be refused without throwing or relying on exceptions.

The result type should be a small C++98-friendly value:

```cpp
enum StateSetStatus
{
  STATE_SET_OK,
  STATE_SET_UNCHANGED,
  STATE_SET_REJECTED,
  STATE_SET_STALE_OWNER,
  STATE_SET_NO_TRACKER,
  STATE_SET_TRANSACTION_FAILED
};

struct StateSetResult
{
  StateSetStatus status;
  const char *message;

  bool ok() const;
};
```

The exact status list can change. The important contract is that failure is
typed, local, and easy to forward to Flow.

## Immediate And Transaction Results

Some failures are known immediately:

- the target state is not mutable;
- the owner/lifetime token is stale;
- a policy gate rejects the value;
- a stale async result is discarded;
- a required tracker/owner is missing.

Other failures are only known when the StateTracker transaction ends:

- derived-state propagation does not settle;
- the update iteration limit is reached;
- a deferred callback marks State dirty during commit;
- an explicit interaction graph detects a cycle.

Therefore the model likely needs two related result surfaces:

```cpp
StateSetResult MutableState<T>::trySet(const T &value);
StateTransactionResult PushStateTracker::endChecked();
```

`StateTrackerGuard` can keep the simple RAII path, while checked Flow adapters
can use the richer result when they need to surface failure.

## Flow Integration

Checked State updates should be easy to adapt into Flow:

```cpp
Flow()
  .from(input)
  .map(...)
  .tryApplyTo(output)
  .onSuccess(...)
  .onFailure(...);
```

The adapter should map both immediate `trySet` failure and transaction-end
failure into the same Flow failure path. This keeps UI interaction code explicit
without pushing app authors toward shared mutable flags.

Conceptually:

```cpp
StateSetResult setResult = output.trySet(nextValue);
if (!setResult.ok())
{
  failureState.set(setResult);
  failureEmitter.emit();
}
```

For long-lived or multi-step flows, transaction failure should include enough
diagnostic context to identify the State, tracker phase, Flow step, interaction
group, and owner-side apply step when available.

## Tracker Responsibilities

StateTracker should remain the place that detects transaction-level hazards:

- dirty propagation limit reached;
- circular dependency propagation;
- dirty State created during commit;
- explicit interaction group cycle;
- transaction did not settle.

It should not decide application recovery policy by itself. It should record or
return a typed transaction result, and Flow or the owner-side caller decides
whether to discard, retry, quantize, throttle, show an error, or accept a
fallback value.

## Default `set()` Policy

The ordinary `set()` API should remain available and fast. It is appropriate for
simple internal UI state, tests, and low-risk single-owner updates.

Checked APIs are required when failure is meaningful to the caller:

- bidirectional values;
- cross-Boundary or owner-side apply;
- continuous input;
- async result application;
- validation;
- known feedback loops;
- external platform/resource mutation.

This keeps simple code simple while making risky code visibly explicit.

## Milestone

Target an initial implementation around `0.0.3`, after the first `0.0.2`
interaction-pattern samples validate sliders/converters and State/Flow update
gates. The first implementation can be small:

1. Define `StateSetResult` and `StateTransactionResult`.
2. Add `trySet` to `MutableState<T>`.
3. Add checked StateTracker transaction result access.
4. Add a focused Flow adapter or test-only prototype.
5. Cover update-loop and commit-dirty behavior with unit tests.

