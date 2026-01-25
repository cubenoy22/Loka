# Reactive State Quick Guide

This doc is a minimal, practical reference for `State`, `MutableState`, and `EmitterState`.
It focuses on what you need to wire UI updates without exceptions or RTTI.

## Core Types

- `State<T>`: read-only value (can be `DerivedState`).
- `MutableState<T>`: read/write value with change notifications.
- `EmitterState`: event-only state, use for UI events.
- `DerivedState<T>`: computed state from dependencies.
- `BoundState<T>`: state handle tied to a boundary owner + tracker.
- `loka::core::StateTrackerGuard`: RAII transaction for mutations.
- `loka::core::Managed<T>`: intrusive ref-counted handle for platform resources.

## Basic Flow

Use `declareStates()` for UI-owned state.

```cpp
#include "core/State.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "loka/core/String.hpp"

class CounterComponent
{
public:
  CounterComponent() : count_(), clicked_(), tracker_() {}

  void attach(loka::core::scene::NodeComposition &c)
  {
    c.declareStates().state(count_, 0);
  }

  void handleClick()
  {
    loka::core::StateTrackerGuard guard(&this->tracker_);
    this->count_.set(this->count_.get() + 1, true);
  }

  loka::core::MutableState<int> count_;
  loka::core::EmitterState clicked_;
  loka::core::PushStateTracker tracker_;
};
```

## BoundState

`BoundState<T>` wraps a `MutableState<T>` with a tracker and owner.
Use it when you get state from `NodeComposition::useState()` or `declareStates()`.

```cpp
#include "core2/scene/NodeComposition.hpp"

class DemoBoundary : public loka::core::scene::StaticCompositionNodeFor<DemoBoundary>
{
public:
  typedef loka::core::scene::StaticCompositionPropsFor<DemoBoundary> PropsType;
  DemoBoundary(const PropsType &p)
      : loka::core::scene::StaticCompositionNodeFor<DemoBoundary>(p),
        counter_()
  {
  }

  virtual void composeNode(loka::core::scene::NodeComposition &c)
  {
    c.declareStates().state(counter_, 0);
  }

  void increment()
  {
    if (!counter_.isValid())
    {
      return;
    }
    counter_.set(counter_.get() + 1, true);
  }

private:
  loka::core::scene::BoundState<int> counter_;
};
```

## State Ownership

Keep ownership with the longest-lived object to avoid dangling references.

- Boundary-owned state: created via `useState`/`declareStates`, destroyed with the Boundary.
- Props-held state: owned by parent/Scene; nodes can come and go safely.
- `EmitterState`: typically owned by Scene/Window-level objects.

## StateStream

`StateStream` creates derived values or binds one state into another.
Avoid it on 68k hot paths unless justified.

```cpp
#include "loka/dsl/StateStream.hpp"

struct ToLabel
{
  typedef loka::core::String Result;
  Result operator()(int value) const
  {
    return loka::core::String::Literal("Count: ") + loka::core::String::FromInt(value);
  }
};

loka::core::scene::BoundState<int> count;
loka::core::scene::BoundState<loka::core::String> label;

count.stream().map(ToLabel()).set(label, true);
```

## Managed

`loka::core::Managed<T>` is a small ref-counted handle for platform resources.
It is often used for shared caches or handles with custom release logic.

```cpp
#include "core/Managed.hpp"

struct Thing
{
  int value;
  explicit Thing(int v) : value(v) {}
};

static void ReleaseThing(Thing *thing, void *)
{
  delete thing;
}

loka::core::Managed<Thing> a = loka::core::Managed<Thing>::Wrap(new Thing(42), &ReleaseThing);
loka::core::Managed<Thing> b = a; // ref-counted copy
```

## ErrorSink (Draft)

ErrorSink is the planned unified error flow for loaders and headless components.

- Producers push `ErrorEvent` into a sink.
- Consumers either poll (`tryPop`) or bind via an `EmitterState`.
- Loaders should avoid opening dialogs directly; use the sink instead.

## EmitterState (Events)

Bind events with `deferBind` (preferred for UI updates).

```cpp
clicked_.deferBind(&CounterComponent::ClickThunk, this);
```

## DerivedState

Use derived values instead of manual recompute when possible.

```cpp
struct DoubleEval : public loka::core::DerivedState<int>::EvalFn
{
  explicit DoubleEval(loka::core::State<int> *src) : src_(src) {}
  virtual int operator()() { return src_ ? src_->get() * 2 : 0; }
  loka::core::State<int> *src_;
};

loka::core::DerivedState<int> doubled(&count_, new DoubleEval(&count_));
```

## Binding Notes

- Prefer `deferBind` in UI/scene contexts.
- `bind` is for immediate recompute when required.
- Avoid `dynamic_cast` in DSL/scene code.

## Mutation Rules

- Always wrap `MutableState<T>::set()` in `loka::core::StateTrackerGuard`.
- Use `forceUpdate=true` when you need to re-emit the same value.

## Classic Notes

- No exceptions.
- Keep props stable; avoid transient data in DSL props.
- Use `NodeKind` or `asXxx()` for type checks (no RTTI).
