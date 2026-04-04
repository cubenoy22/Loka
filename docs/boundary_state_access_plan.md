# Boundary State Access Plan

Status: active design note

This note captures the next ownership/access tightening after the constant-prop
state binding bug fixed in `93a3279`.

Current implementation status:

- `findBoundary()` is now direct-parent-only
- borrowed boundary lookup now returns `FoundBoundary<T>`
- borrowed lookup is read-only (`const` facade access)
- `currentBoundary()` now exists as the owner-side counterpart
- `BoundState` mutable escape hatches are marked `dangerously*`

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

The final API does not need to expose direct mutable helpers on
`currentBoundary()` immediately, but it should preserve those semantics.

## Parent Facade Sketch

The preferred cross-boundary pattern is:

```cpp
class ParentBoundaryFacade
{
public:
  loka::core::State<loka::core::String> *titleState() const;
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
- `currentBoundary()` exists, but still needs a more opinionated owner-side API

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
- mostly done: 3
- started: 4
- pending: 5
- pending: 6

## Open Questions

Questions still worth deciding before implementation:

- whether `currentBoundary()` should expose direct mutable access or a narrower
  owner-only writer helper
- whether borrowed access should return raw `State<T>*` or a lightweight
  borrowed wrapper
- how much of the facade should be handwritten vs generated/templated
- how strongly component code should be prevented from storing owner-side
  mutable handles beyond compose-time use
- whether `declareStates()` should eventually return a more owner-scoped handle
  than raw `BoundState<T>` storage in components

## Mutability Rules

Planned direction:

- `declareStates()` is the canonical owner-side declaration path
- owner-side mutable updates should happen through boundary-owned state only
- components should not keep long-lived mutable owner handles by default
- children should signal intent upward instead of mutating parent state directly

In practice this means:

- owner path: writable
- borrowed path: read-only
- dangerous escape hatch: explicit and noisy

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

## Guard Rails

Rules alone are not enough. The design should also add mechanical constraints.

Desired guard rails:

- make owner access and borrowed access different API paths
- avoid returning raw mutable boundary state from non-owner APIs
- make `findBoundary` return a restricted facade-like type, not a raw boundary
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
