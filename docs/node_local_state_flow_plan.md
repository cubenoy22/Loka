# Node-local State and Flow Plan

## Goal

Loka should make ordinary UI code safe by default without hiding lifecycle ownership.
Node-local state, flow, and stream/reaction bindings should be declared on the Node that owns their meaning.
The framework should connect them to the current Boundary owner/tracker during attach, and release bindings when the owning Node is destroyed.

This keeps the standard API close to GC-like ergonomics while preserving deterministic cleanup and static resource visibility.

## Current Problems

- `StateStream::map().set()` creates heap-owned binding objects without a clear owner.
- Example code can look stack-only even when it installs long-lived deferred bindings.
- `c.declareStates()` works, but it reads as Boundary composition plumbing even when the state semantically belongs to one Node.
- Manual `bind` / `unbind` patterns are easy to get wrong, especially in C++98 code with early returns and retained/dynamic child lifetimes.
- Static tooling cannot reliably estimate state/flow/reaction resource cost when ownership is implicit or created through ad hoc helpers.

## Ownership Model

### Allocation policy

App-facing Loka code should avoid raw `new` whenever practical.

The standard API should hide allocation behind DSL/builders that immediately attach the allocation to a lifecycle-aware owner:

- Node member registration
- Boundary state owner/tracker
- slot-owned reaction/flow runtime
- repository/cache owner
- platform/native context owner

Internal implementation may still allocate, but the allocation must have an obvious cleanup bucket before it escapes the expression that created it. A builder that allocates without registering ownership is not acceptable for production UI APIs.

Manual `new` remains an expert/engine-level escape hatch, but it should not be the normal shape of example or application code.

External libraries are different. Code outside the Loka DSL may be written in normal C++, Rust, Swift, or platform-native styles. The boundary rule is that resources crossing into Loka should be wrapped by lifecycle-aware adapters before app/UI code observes them.

Future adapter direction:

- wrap external handles in small RAII/resource adapter types
- register those adapters with a Node, Boundary, repository, or platform owner
- expose borrowed/read-only or explicit command surfaces to UI code
- keep raw external handles out of ordinary props/state surfaces where practical

For resources such as SQLite connections, statements, transactions, files, or media decoders, Loka should eventually provide a lightweight lifecycle resource helper. This can feel like try-with-resources even though C++ already has RAII: the value is not only destructor cleanup, but also attaching the resource to a meaningful Loka lifecycle owner.

### Node-local default

A Node may own:

- `BoundState<T>` members
- `FlowSlot` members
- future `ReactionSlot` / stream binding members

The Node is the semantic owner. The current Boundary remains the allocation/tracker owner unless a more explicit owner is provided.

### Boundary role

The Boundary provides:

- state allocation
- `StateTracker`
- cleanup bucket for state storage
- composition/update boundary

Boundary-owned resources should not become a catch-all bucket for unrelated shared resources. Shared resources should live at the nearest meaningful common owner, root owner, app-level owner, or cache repository.

### Child access

Children should receive live state through read-only surfaces by default:

- `State<T>*`
- `BorrowedState<T>`

Mutable cross-boundary access should remain explicit and rare:

- callbacks
- `EmitterState`
- `dangerously*` escape hatches

### Sharing and promotion

Node-local state should be local by default, but it should not be trapped there forever. Some state starts local and later becomes useful to share with a parent Boundary, a root owner, or a repository-style singleton.

The preferred direction is explicit owner promotion rather than implicit reference counting:

- a Node may declare state locally while it remains semantically Node-owned
- if the state becomes shared, move or re-declare the owner at the nearest meaningful common owner
- parent Boundary / root owner / repository-owned state can be passed back down as `BorrowedState<T>` or read-only `State<T>*`
- broad sharing should prefer a meaningful owner pointer/owner token over anonymous ref-count ownership
- repository-owned shared/global state is the long-term model for cache/resource-like data
- `Managed<T>` may still be useful as a transitional payload-lifetime tool, but it should not replace a meaningful state/resource owner
- long-term APIs should prefer repository/owner-token lifetimes over broad `Managed<T>` propagation where practical

This keeps sharing understandable: the important question is not "who has a reference count?" but "which owner gives this state its lifecycle meaning?"

## Proposed API Shape

### Node-local registration

```cpp
class ExampleNode : public loka::app::scene::StdCompositionNodeFor<ExampleNode>
{
public:
  ExampleNode(const PropsType &p)
      : loka::app::scene::StdCompositionNodeFor<ExampleNode>(p),
        count_(),
        label_(),
        labelReaction_()
  {
    this->state(this->count_, 0);
    this->state(this->label_, loka::core::String::Literal(""));

    this->reaction(this->labelReaction_)
        .from(this->count_)
        .map(FormatCount())
        .to(this->label_);
  }

private:
  loka::app::scene::BoundState<int> count_;
  loka::app::scene::BoundState<loka::core::String> label_;
  loka::app::scene::ReactionSlot labelReaction_;
};
```

Node-local registration should be available through base-class helper methods rather than requiring several dedicated virtual hooks. Constructors are the preferred place for stable Node member registration, but registration is not limited to construction.

Any state/slot registered before destruction becomes managed from that point forward:

- if registered before first attach, it is connected on first attach
- if registered while already attached, it is connected immediately
- if registered while detached but still logically alive, it is connected on the next attach
- detach/reattach/destruction must handle all registered entries consistently
- registering during destruction is invalid and should assert in debug builds

Reaction/flow registration depends on state availability. The API may let users describe a reaction before attach, but the runtime binding must not be installed until all referenced `BoundState<T>` handles have real state storage.

Practical rule:

- state registration records desired state allocation
- attach allocates/connects registered states first
- after state connection, registered reactions/flows are bound
- if a reaction/flow is registered while attached, it may bind immediately only if all source/target states are valid
- if any referenced state is not valid yet, the reaction/flow remains pending and is retried after state connection
- debug builds should assert if a pending reaction/flow cannot be resolved by the end of attach

The registered state list must be idempotent for a logical Node instance:

- first attach registers unbound `BoundState<T>` members with the current Boundary owner/tracker and applies the initial value
- retained detach/reattach must not create a second state or reset the existing value
- if a `BoundState<T>` is already attached, the state helper should skip initial value application
- registering the same `BoundState<T>` member twice should be detected in debug builds
- logical Node destruction is the point where the state lifetime ends

In other words, reattach should reconnect the logical Node as needed, but it should not behave like a fresh state declaration unless a new Node instance was actually created.

### Flow/reaction registration

Long-lived flow or stream/reaction work should be held by an explicit member slot.

```cpp
class ExampleNode : public loka::app::scene::StdCompositionNodeFor<ExampleNode>
{
public:
  ExampleNode(const PropsType &p)
      : loka::app::scene::StdCompositionNodeFor<ExampleNode>(p),
        count_(),
        label_(),
        labelReaction_()
  {
    this->state(this->count_, 0);
    this->state(this->label_, loka::core::String::Literal(""));
    this->reaction(this->labelReaction_).from(this->count_).map(FormatCount()).to(this->label_);
  }

private:
  loka::app::scene::BoundState<int> count_;
  loka::app::scene::BoundState<loka::core::String> label_;
  loka::app::scene::ReactionSlot labelReaction_;
};
```

The exact helper names are still open. The important constraint is that the long-lived owner is the explicit member slot, while the Node base tracks when to attach or clear the slot.

### DSL ergonomics

The explicit slot should not make common reactions verbose. A simple value transform should remain a one-line chain:

```cpp
this->reaction(this->labelReaction_).from(this->count_).map(FormatCount()).to(this->label_);
```

The chain is a declaration of a runtime owned by `labelReaction_`; it should not allocate unowned bindings while building the expression.

The DSL should cover the common `StateStream` shape:

- `from(source)`
- `map(functorOrExpr)`
- future `filter(predicate)` if a clear no-update semantic is defined
- `to(target)`
- optional `when(...)` / `gate(...)` only after the base lifetime model is stable

Mapper ownership should be explicit and simple:

- stateless or small stateful mappers are copied into the slot-owned runtime
- mappers that borrow Node data must only borrow data with the same or longer lifetime than the slot
- borrowing large data should use an explicit wrapper/name such as `borrow(...)` or a documented functor type
- raw pointer captures are allowed only when their lifetime is obvious or the API is marked `dangerously*`
- debug builds should prefer owner/lifetime checks when a mapper references state-like objects

This keeps the ergonomic feel of `stream().map().set()` while making the long-lived owner visible at the start of the expression.

## Slot Rules

Slots should be lifecycle-aware RAII objects.

- `clear()` must be idempotent.
- destructor must call `clear()`.
- `bind()` must clear any previous binding first.
- copy should be disabled.
- source state must be unbound in `clear()`.
- pending callbacks should not write to target after cancellation.
- debug builds should detect double bind, invalid source/target, and owner/lifetime mismatch where practical.

Member declaration order matters in C++:

```cpp
BoundState<int> count_;
BoundState<String> label_;
ReactionSlot labelReaction_; // declared after states, destroyed before states
```

Slots that refer to states should be declared after those states so their destructors run first.

This declaration-order rule is not sufficient by itself. `ReactionSlot::bind` / `FlowSlot::bind` should also validate source and target lifetime in debug builds:

- source and target states should belong to the same Node/Boundary owner as the slot, or to an explicitly longer-lived owner
- binding to a shorter-lived state should assert unless the API is explicitly marked `dangerously*`
- binding to a raw `State<T>*` should go through a debug-checkable wrapper or an explicit unsafe API
- slot clear/destruction should assert or log if the referenced state owner has already been destroyed

The goal is that member order mistakes and accidental short-lived state captures fail during development instead of becoming stale deferred callbacks.

## StateStream Policy

`StateStream` should not be exposed as the ordinary production UI surface in its current form.

It may remain useful internally. The problem is not the stream abstraction itself; the problem is letting app code create long-lived runtime bindings through a stack-looking expression with no visible lifecycle owner.

Short-term:

- remove `StateStream` usage from official examples
- replace with explicit refresh functions or future `ReactionSlot`
- keep `StateStream` available for tests/internal implementation if useful
- avoid documenting `StateStream` as the recommended app-facing API

Long-term:

- allow stream DSL internals to produce a spec/recipe
- make `ReactionSlot` own the runtime binding objects
- avoid `StateStream::set()` creating unowned heap bindings
- expose `ReactionSlot` / Node helper APIs on the public surface instead of raw `StateStream`

## Flow Policy

Flow remains useful for file/image/resource pipelines such as SimpleViewer.

Short-term:

- replace raw `new ViewerFlowChain(...)` with a Node member slot when a slot exists
- keep one-shot stack `FlowChain(...).run()` valid for test/scenario use

Long-term:

- `FlowSlot` owns long-lived flow runtimes
- Node destroy cancels/unbinds the slot
- visibility suspend/resume can be added later as a policy

## Example Migration Order

1. HelloWorld
   - remove `StateStream`
   - add explicit fruit message refresh or `ReactionSlot` once available
   - migrate ordinary state declaration to Node-local API after it exists

2. Tutorial Step4
   - remove `StateStream`
   - keep tutorial wording aligned with the replacement API

3. SimpleViewer
   - replace raw `ViewerFlowChain *flow_` with `FlowSlot`
   - review `dangerouslyUseState` use separately

4. MineSweeper and remaining examples
   - migrate `c.declareStates()` where Node-local ownership is clearer

## Manual API Policy

Standard declaration APIs should be preferred. They give Loka:

- deterministic cleanup
- state/resource ownership
- tracker wiring
- static resource estimation
- future platform projection control

Manual APIs remain available for engine-level, game, media, and platform-specific work. Manual ownership means manual responsibility. These APIs should be named or documented as explicit escape hatches.

## Static Tooling Direction

Declaring state/flow/reaction as Node or Boundary members makes resource estimation possible.

Future tooling can estimate:

- state count and type mix
- flow/reaction count
- derived state count
- binding count
- logical memory per Node/Boundary
- retained hidden subtree memory
- dynamic/unbounded resource use

Dynamic helpers such as `dangerouslyUseState` should be reported as dynamic or unbounded unless annotated.
