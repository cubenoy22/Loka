# Boundary State Access Plan

Status: archived design note. The implemented parts have been folded into the
current state ownership model and `docs/ProgrammingGuide.md`.

This note captures the next ownership/access tightening after the constant-prop
state binding bug fixed in `93a3279`.

Current implementation status:

- `findBoundary()` is now direct-parent-only
- borrowed boundary lookup now returns `FoundBoundary<T>`
- borrowed lookup is read-only (`const` facade access)
- `BorrowedState<T>` now exists as a lightweight borrowed-state wrapper for
  facade/props surfaces
- `currentBoundary()` now exists as the owner-side counterpart
- `currentBoundary()` is now owner-access-only and no longer exposes the raw
  boundary pointer
- `currentBoundary().state(...)` now exposes only owner-matched `get()/set()`
- `BoundState` mutable escape hatches are marked `dangerously*`
- `BoundState` no longer implicitly converts to `State<T>*`
- `BoundState` owner/tracker access now lives behind `dangerously*` naming
- `NodeComposition::useState()` is now `dangerouslyUseState()`
- boundary/menu ad hoc state creation helpers are also marked `dangerously*`
- `BoundaryPropsFor<T>` now has `BoundaryPropValueRules`, `borrowed(...)`, and
  `shared(...)` helpers to steer custom boundary props toward borrowed/shared
  inputs instead of owned mutable state

The goal is not to maximize flexibility. The goal is to keep the app-facing
state model small, explicit, and difficult to misuse.

## Problem

Loka currently allows too many ways to pass and mutate state:

- raw `MutableState<T>` can leak into app/UI code too easily
- props can blur constant values and live state
- a child can too easily end up holding mutable state that conceptually belongs
  to a parent boundary
- cross-boundary access risks turning into hidden graph traversal

Even when the immediate bug is fixed, this freedom increases the chance of:

- ownership drift
- lifecycle inversion
- accidental parent mutation from children
- platform bindings observing things that should be treated as constants

## Target Model

Mutable state in app/UI code should come from only two places:

1. boundary-owned local state
2. repository-owned shared/global state

Everything else should be read-only input, constant props, or an explicit
facade.

This keeps ownership aligned with the existing gravity rule:

- parent owns child-facing state/data by default
- child reads props or explicit borrowed state
- child does not become the owner of parent state
- ownership does not move "upward" or "inward" at runtime

## Ownership Classes

Treat state-like values as three different classes:

1. Props-owned constant value
   - owned by props/definition/node
   - not observed as live state
   - not bound/unbound through NativeContext live-state paths

2. Borrowed live state
   - owned elsewhere
   - readable and observable
   - not writable through child/local convenience APIs
   - may be surfaced as `BorrowedState<T>` when a facade or `BoundaryProps`
     wants a read-only state-shaped input without exposing mutable ownership

3. Boundary-owned mutable state
   - declared by the owning boundary
   - writable only by the owning boundary
   - exposed downward as read-only unless an explicit facade says otherwise

## Boundary Access Rules

Planned direction:

- `NodeComposition` is the main access point
- `currentBoundary()` means the currently composing owner boundary
- `findBoundary()` is restricted to the direct parent boundary only
- sibling/uncle/cousin/multi-hop traversal is not a normal API
- `findBoundary().findBoundary()` should not be possible

Cross-boundary coordination should use:

- props
- parent-owned facades
- borrowed read-only state

not arbitrary boundary graph traversal.

## API Sketch

This is still a sketch, but it is now close to the current code direction:

```cpp
virtual void composeNode(loka::app::scene::NodeComposition &c)
{
  c.currentBoundary();

  c.declare(
      loka::app::Text(c.findBoundary<ParentBoundary>().facade().titleState()));
}
```

More concretely:

- `c.currentBoundary()` is the owner-side path
- `c.findBoundary<ParentBoundary>()` is the direct-parent borrowed path
- borrowed lookup should expose only approved surface
- normal borrowed access should not return writable foreign `BoundState`
- borrowed facades may expose read-only `State<T>*` when live reflection is
  needed, but mutation still stays on the owner path

The final API does not need to expose direct mutable helpers on
`currentBoundary()` immediately, but it should preserve those semantics.

## Parent Facade Sketch

The preferred cross-boundary pattern is:

```cpp
class ParentBoundaryFacade
{
public:
  loka::app::scene::BorrowedState<loka::core::String> titleState() const;
  bool canShowDetails() const;
};
```

Then the child uses the facade, not the raw boundary:

```cpp
ParentBoundaryFacade parent = c.findBoundary<ParentBoundary>().facade();
```

Important restrictions:

- facade defaults to read-only
- facade should not expose generic boundary lookup
- facade should not expose arbitrary mutable state handles
- facade should stay narrow and task-specific

## Write Access Sketch

The intended write model is:

- owner boundary code can update its own `BoundState`
- child code does not update ancestor-owned `BoundState` directly
- child code raises intent through callback/event/facade method
- parent boundary performs the actual state mutation

That means code like this should be considered invalid by default:

```cpp
c.findBoundary<ParentBoundary>().state(parentCount_).set(42);
```

If a dangerous cross-boundary write escape hatch is ever needed, it should be
rare, explicit, and named accordingly.

## Mechanical Restriction Options

Several implementation options are possible. The simplest good direction is:

1. `findBoundary` returns a restricted wrapper such as `FoundBoundary<T>`.
2. `FoundBoundary<T>` exposes only facade/read-only APIs.
3. `FoundBoundary<T>` does not expose another `findBoundary`.
4. owner-only mutable access stays on the `currentBoundary` path.

Items 1-4 are now partially implemented:

- `FoundBoundary<T>` exists
- `FoundBoundary<T>` is read-only
- `findBoundary()` is direct-parent-only
- `currentBoundary()` now exposes only owner-oriented access
- `currentBoundary().state(...)` exposes owner-matched `get()/set()` only
- `BorrowedState<T>` and `BoundaryPropsFor<T>::borrowed/shared` now exist as
  the first small compile-time/mechanical steer toward safe cross-boundary
  props

This gives:

- near-zero runtime cost
- compile-time restriction of common misuse
- no need for RTTI-heavy access
- a natural place to attach dangerous escape hatches separately

## Migration Order

To keep the change reviewable, the likely order is:

1. document and freeze the boundary access rules
2. restrict `findBoundary` to direct parent only
3. introduce restricted parent facade/wrapper return
4. remove normal writable foreign-state access
5. tighten `declareStates()` ownership expectations
6. add regression tests for forbidden access patterns

Progress so far:

- done: 1
- done: 2
- done: 3
- done: 4
- mostly done: 5
- started: 6

## Open Questions

Questions still worth deciding before implementation:

- whether raw `State<T>*` should remain acceptable on public borrowed surfaces,
  or whether `BorrowedState<T>` should become the preferred/required wrapper
- how much of the facade should be handwritten vs generated/templated
- how strongly component code should be prevented from storing owner-side
  mutable handles beyond compose-time use
- whether `declareStates()` should eventually return a more owner-scoped handle
  than raw `BoundState<T>` storage in components

## Mutability Rules

Planned direction:

- `declareStates()` is the canonical owner-side declaration path
- `NodeComposition::dangerouslyUseState()` exists only as an escape hatch
- `BoundaryNode::dangerouslyUseState()` and `dangerouslyUseManagedState()` are
  also escape hatches, not the default composition path
- menu composition state creation also uses `dangerouslyUseState()` naming for
  the same reason
- owner-side mutable updates should happen through boundary-owned state only
- components should not keep long-lived mutable owner handles by default
- children should signal intent upward instead of mutating parent state directly

In practice this means:

- owner path: writable
- borrowed path: read-only
- dangerous escape hatch: explicit and noisy

Current intended interpretation:

- ordinary component/boundary local state should prefer `declareStates()`
- direct ad hoc state creation during compose should be treated as exceptional
- if a call site reaches for `dangerouslyUseState()`, that should trigger design
  review rather than be treated as a normal pattern
- normal `common/` and `example/` DSL code should ideally have zero
  `dangerously*` state creation callsites
- keeping `BoundState<T>` as a component member is acceptable only for that
  component's own boundary-owned state declared through `declareStates()`
- `BoundState<T>` should not be used as a cross-boundary transport type

## Facade Direction

When a child needs selected parent capabilities, prefer a narrow facade instead
of exposing the boundary itself.

That facade should:

- expose only the approved surface
- default to read-only state access
- avoid exposing generic re-traversal APIs
- avoid becoming a hidden ownership-transfer mechanism

`Managed<T>` remains explicit shared access from a larger scope to a smaller
scope. It should not imply lifecycle inversion or ownership transfer.

## BoundaryProps Direction

`BoundaryProps` is the normal parent-to-child input path.

That means the main restriction target is not boundary existence itself, but
what kinds of values are allowed to cross a boundary through props.

Current intended direction:

- allow plain values
- allow read-only `State<T>*`
- allow `BorrowedState<T>`
- allow `Managed<T>` as explicit shared access
- allow narrow facades
- disallow `BoundState<T>`
- disallow raw `MutableState<T>*`
- disallow owner-specific mutable handles

The current `BoundaryPropValueRules` helper is only a first compile-time hook.
The long-term goal is to make correct `BoundaryProps` setters easy to write and
incorrect ones noisy enough to catch in review or at build time.

## Boundary Lifetime Note

Current default model:

- boundary lifetime follows scene/tree structure
- boundary lifetime does not automatically follow native view visibility
- hiding a projection is not the same thing as detaching a boundary

This is intentional. It keeps ownership aligned with the logical scene
structure and avoids tying app state to backend-specific view churn.

Possible future extension:

- opt-in boundary lifecycle policies such as scene-owned vs view-scoped

That is intentionally deferred. For now, if a subtree should preserve or clear
editing state differently from its view lifetime, prefer solving it through
state placement (for example a headless owner or parent-owned model) rather
than automatic boundary destruction tied to visibility.

## Guard Rails

Rules alone are not enough. The design should also add mechanical constraints.

Desired guard rails:

- make owner access and borrowed access different API paths
- avoid returning raw mutable boundary state from non-owner APIs
- make `findBoundary` return a restricted facade-like type, not a raw boundary
- keep `declareStates()` as the normal owner-side allocation path
- make dangerous escape hatches obvious, similar in spirit to
  `dangerouslySetInnerHTML`
- add debug assertions for owner-only mutation when practical
- add headless regression tests for ownership/access rules

## Minimal Next Steps

1. Restrict `findBoundary` to direct-parent access only.
2. Make parent lookup return a restricted facade or equivalent wrapper.
3. Keep owner-side mutation on the `currentBoundary` path only.
4. Prevent normal child code from obtaining writable foreign `BoundState`.
5. Audit other props besides `Text` / `Button` / `Cell` for constant-vs-live
   ambiguity.
6. Add tests for:
   - constant props are not observed as live state
   - borrowed boundary access is read-only
   - child cannot mutate parent-owned boundary state through normal APIs

## Explicit Non-Goals For Now

- full arbitrary boundary graph lookup
- sibling-to-sibling direct mutation
- multi-hop boundary traversal
- maximizing generic state plumbing flexibility

The intent is to keep the model smaller first, then widen it only when a clear
need survives review and measurement.
