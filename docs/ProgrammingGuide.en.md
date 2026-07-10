# Loka Programming Guide

Target version: Loka `0.0.1`

Japanese version: [ProgrammingGuide.md](ProgrammingGuide.md)

This guide explains how to write Loka applications and how to think about
Loka's state, ownership, composition, and platform projection model.

The Japanese version of this guide is currently the most detailed design note.
This English edition is written as a readable programming guide rather than a
line-by-line translation.

## Introduction

Loka is a system for building a logical UI structure and state transition model,
then projecting the result into each operating system's native environment.

It is related to declarative UI and reactive state systems, but its assumptions
are different. Loka is designed to work under constraints where garbage
collection, exceptions, modern language features, and large opaque runtimes may
not be available.

Loka therefore emphasizes:

- explicit state ownership
- traceable update flow
- small reusable concepts
- compile-time misuse detection where practical
- no reliance on exceptions or garbage collection
- code that can scale down to old systems without losing the application model
- meaningful application-facing APIs

The goal is not to build the smallest possible C-style UI wrapper. The goal is
to make modern, professional, declarative applications possible while keeping
ownership, lifecycle, and native projection inspectable.

The central concepts are:

- `State`
- `Node`
- `Boundary`
- `Props`
- `Flow`
- platform projection

These concepts are reused across UI, events, async-style workflows, native
reflection, and future resource management. The same vocabulary should explain
where a value lives, who updates it, who observes it, and who cleans it up.

## Who This Guide Is For

This guide is useful if you:

- have used declarative UI frameworks
- know C++ and want a lightweight UI/application model
- can read Loka DSL but want to understand the design intent
- care about old platforms, explicit ownership, or predictable lifecycle

## Reading Order

Start with this flow:

1. `State` holds application facts.
2. UI reads those facts.
3. Events update `State`.
4. Only affected areas are recomposed, reflected, laid out, or redrawn.

This is the core of Loka.

## 1. Basic Philosophy

### UI Is A Result Of Values

In Loka, UI is not primarily something you mutate procedurally. It is the result
of current state.

Examples:

- whether a button is enabled
- what a label displays
- whether a details panel is visible
- which document tab is active

These should usually be represented as state or as values derived from state.

The native platform API is not the center of the framework. Native controls,
menus, windows, and drawing are projection targets. The logical UI and state
model are the source of truth.

### Traceability Before Magic

Loka avoids hiding too much behavior inside a large runtime. Convenience is
important, but ownership and update flow must remain traceable.

When something redraws or recomposes, it should be possible to answer:

- which state changed?
- who owns that state?
- which boundary observed it?
- which platform projection path ran?

This matters on every platform, and it matters even more on old systems where
debugging tools, memory, and CPU time are limited.

### Reactive Does Not Mean Unlimited Automation

Loka is reactive, but it does not try to make every update automatic through an
unbounded dependency graph.

Instead, application code should choose:

- the right state type
- the right owner
- the right boundary
- the right projection/update scope

This makes small examples a little more explicit, but it keeps larger
applications easier to reason about.

## 2. Start From State

Loka code usually becomes clearer when you first ask: what facts can change?

For a counter, the important facts are:

- the current count
- the increment event
- the display text derived from the count

The UI reads these facts. Events update them.

```cpp
loka::core::MutableState<int> count(0);
```

`count` is the current value. A button event can update it, and a label can read
or derive text from it.

Not every visual value needs its own mutable state. If a value can be derived
from another state, prefer deriving it over introducing another mutable owner.

## 3. Node And Boundary Overview

`Node` is the logical unit of UI.

`Boundary` is also a kind of `Node`, but it adds ownership and update scope. A
Boundary owns state storage, state tracking, composition/update boundaries, and
resource lifetime decisions associated with its subtree.

Conceptually:

```text
Scene
|
+-- Boundary: WindowRoot
|   state:
|     - selectedTab
|     - windowTitle
|
|   nodes:
|     - ToolbarNode
|     - TabGroupNode
|     - StatusBarNode
|
|   +-- Boundary: EditorTab
|   |   state:
|   |     - documentText
|   |     - cursorPosition
|   |
|   |   nodes:
|   |     - TextEditorNode
|   |     - FindPanelNode
|   |
|   +-- Boundary: PreviewTab
|       state:
|         - zoom
|         - renderStatus
|
|       nodes:
|         - PreviewCanvasNode
|         - PreviewToolbarNode
```

Important distinctions:

- `Node` expresses UI meaning.
- `Boundary` is a `Node` plus state ownership and update scope.
- Node-local state is declared on the Node but stored/tracked through the
  attached Boundary owner.
- A child should not casually become the owner of parent-facing state.
- Cross-boundary sharing must be explicit.

This is why `this->state(...)` does not mean "the Node heap-allocates arbitrary
state by itself." It means the Node declares a meaningful local state handle and
the attached Boundary provides the storage/tracker/lifecycle.

## 4. Main State Types

### `State<T>`

`State<T>` is readable state. UI code can read it, and platform projection can
observe it when the logical layer marks it as live state.

Use it for values such as:

- label text
- enabled flags
- selected indexes
- read-only values exposed to children

### `MutableState<T>`

`MutableState<T>` is writable state.

```cpp
loka::core::MutableState<loka::core::String> title(
    loka::core::String::Literal("Hello"));
```

In ordinary UI/DSL code, mutable updates should be done through the correct
owner and inside a `StateTracker` transaction. Do not pass raw mutable state
around just because a value needs to change.

### `EmitterState`

`EmitterState` represents an event rather than a stored value.

Use it for one-shot notifications:

- a button was clicked
- a menu item was selected
- a reload was requested

Keeping events separate from stored values avoids mixing "what is true now" with
"what just happened."

### `NodeState<T>`

`NodeState<T>` is a Node-owned state handle backed by the attached Boundary
owner.

It exists to keep these facts together:

- the state has meaning on this Node or Boundary
- the storage belongs to the active owner
- the tracker/lifecycle are not arbitrary

Use `this->state(...)` for ordinary Node-local state:

```cpp
MyNode(const PropsType &p)
    : Base(p),
      count_(),
      label_()
{
  this->state(this->count_, 0);
  this->state(this->label_, loka::core::String::Literal("Count: 0"));
}
```

Use `declareStates(...)` when a Node has many Node-local states and batching the
declaration makes registration cheaper or clearer. For a small number of local
states, prefer `this->state(...)`.

## 5. Boundary-First Ownership

Mutable state is not "something anyone can touch." In Loka, the normal owner is
a Boundary or a long-lived repository/global owner.

The common routes are:

1. Node-local state backed by the attached Boundary owner.
2. Explicit repository/shared state for long-lived application facts.

Avoid passing raw `MutableState<T>*` across unrelated components as a mutation
channel. It makes the dependency graph flat and hard to debug.

### `currentBoundary()`

`currentBoundary()` is an owner-side path. It is for code operating on the
Boundary currently being composed.

It is not a general search API.

Use it when the owner needs to access its own Boundary-owned state or services.

### `findBoundary()`

`findBoundary()` is a borrowed direct-parent path.

It should not be treated as sibling traversal, multi-hop lookup, or a hidden
service locator.

Good use:

- a child reads a narrow facade exposed by its direct parent Boundary
- a child observes a parent-owned read-only state

Bad use:

- walking through multiple ancestors to find something convenient
- mutating a parent through an undocumented channel
- making sibling components implicitly depend on each other

## 6. Boundary Props And Shared Access

Parent-to-child data should normally flow through `BoundaryProps` or regular
Props.

If a child needs data owned by a parent, the parent should pass a meaningful
surface:

- a read-only state
- a facade
- a command/event emitter
- immutable props

Use `Managed<T>` only when stabilizing payload lifetime is the actual problem.
`Managed<T>` does not replace ownership design. It can keep an object alive, but
it does not explain who conceptually owns the value, who may mutate it, or when
the application should consider it obsolete.

## 7. Dangerous APIs Are Escape Hatches

APIs named `dangerously*` are not normal application APIs.

They exist for framework internals, tests, or exceptional design cases where the
caller is explicitly taking responsibility for ownership and lifecycle.

A new `dangerously*` callsite should be treated as a design event:

- Why is the normal owner path insufficient?
- Who owns the value?
- Who cleans it up?
- What prevents dangling references?
- Can this be expressed as `state()`, `declareStates()`, Props, or a facade?

## 8. `StateTracker`

`StateTracker` records which state changes affect which composition/update paths.

It is the reason Loka can avoid treating the whole UI as one undifferentiated
tree. State updates should be made inside a tracker transaction so the framework
can see what changed and decide what to update.

In ordinary code, prefer RAII guard helpers instead of manually opening and
closing transactions.

Future versions should expose better error/result handling for failed or
looping updates so Flow can react to state update failures without relying on
ad-hoc checks.

## 9. Flow

Loka cannot assume `async`/`await`, modern closures, or a garbage-collected task
runtime. `Flow` is the framework-level way to describe multi-step logic while
remaining compatible with old C++ and old platforms.

Use Flow when logic has steps:

- input
- validation
- conversion
- state update
- result emission
- failure handling

For long-lived chains owned by a Node or Boundary, store them in `FlowSlot<T>` or
an equivalent lifecycle-aware slot. One-shot stack Flow usage should stay limited
to tests or bounded local operations.

Do not share mutable state between unrelated Flow instances just because several
paths need to update a value. Prefer:

- Flow-owned input state
- dedicated result state
- read-only input state
- `DerivedState`
- emitter adapters
- an explicit owner facade

This keeps update loops and lifecycle relationships visible.

## 10. Bidirectional Input And Update Loops

Bidirectional UI can accidentally create loops:

1. A control updates state.
2. State reflection updates the control.
3. The control emits another change.
4. The same state changes again.

Platform contexts should guard against this with explicit flags such as
`applyingFromState_` or `updatingFromControl_`, and state writes should be
tracked.

For numeric controls, sliders, conversions, or formatted text, prefer explicit
input/result state or a Flow adapter instead of letting two mutable states
blindly write to each other.

Future work should include better loop detection and state update result APIs.

## 11. StdComposition And Boundary

StdComposition is Loka's current standard composition model. It deliberately
keeps the core algorithm small and predictable.

It should not be understood as "the only possible composition algorithm." Loka's
state and boundary model should allow other composition strategies, including
game, media, test, or platform-specific strategies, without making all
composition equal to StdComposition.

The important rule is:

```text
Composition is owned by Boundary.
State tracking is owned by Boundary.
Projection reflects the logical result.
```

### `Show()`

`Show()` should be understood as an attach/detach mechanism rather than merely
an `if` statement.

For `0.0.1`, retained attach/detach behavior is still being stabilized. Future
versions should define whether hidden subtrees keep state, release state, or use
an explicit policy such as a Boundary section.

The design goal is that memory and lifecycle are visible from the DSL structure.

## 12. DSL And Composition

Loka's DSL declares structure.

It should express application intent:

```cpp
VStack()
    << Text(title.state())
    << Button("Save").onClick(saveEmitter.state());
```

Prefer chained DSL composition when it keeps the structure visible. Avoid local
temporaries whose only purpose is to assemble a tree in a less readable way.

Use helper functions returning definitions or fragments when you want to inline
structure without creating a new lifecycle boundary.

Use a Boundary when you need:

- independent state ownership
- independent update tracking
- a lifetime boundary
- a meaningful composition scope

### Props And Definitions

`Props` is the full API surface. `Definition` setters are shorthand for common
DSL callsites.

Do not duplicate every field as a shorthand setter. For uncommon or advanced
fields, construct `Props` explicitly.

Constant props and live state must stay distinct:

- constant text should be owned by props/definition storage
- live text should be passed as `State<T>*`
- platform code should bind only values that the logical layer classified as
  live state

This avoids turning every literal into a global or shared `State<T>`.

## 13. Events And Updates

Prefer `deferBind` for UI reflection and lazy updates. Use `bind` only when
immediate recompute is required.

State updates should be treated as transactions. A mutation should not be a
random write to a widely shared object. It should have:

- a clear owner
- a clear transaction/update context
- a clear projection path

## 14. Platform Projection

The logical UI is the truth. Platform code projects it into native objects.

The same logical structure should be able to target different platforms:

- Toolbox
- Win32
- Cocoa
- future Linux or embedded targets

Platform code should avoid re-deciding application semantics. It should consume
the logical model, bind live states that were already classified as live, and
reflect changes into native controls.

This separation is what lets Loka keep one application model across many eras
and platforms.

## 15. Dialogs, Windows, And App Scope

Dialogs and windows are not just controls. They interact with application-level
policy, native modality, focus, menus, and platform conventions.

Future APIs should make it clear whether a dialog is:

- an app-level default service
- a window-scoped native dialog
- a custom declarative UI subtree
- a linked optional feature

The DSL should make it hard to accidentally declare multiple competing native
dialogs when the platform expects one active dialog.

## 16. Ownership And Resource Management

Loka should make ordinary application code feel like it has modern lifecycle
management while remaining compatible with C++98 and old platforms.

The intended direction is:

- resources have explicit owners
- temporary resources can be released on the next tick
- UI-owned resources can be retained by the owning Boundary
- shared immutable resources can live in caches or repositories
- mutable resources expose a clear mutable phase and cleanup path

Avoid designs where `Managed<T>` or reference counting becomes the only answer.
Reference stability is useful, but it should not erase ownership meaning.

## 17. Mutability

Prefer immutable completed values for:

- props
- snapshots
- facts
- plans
- result objects

Use builders, local temporaries, or explicit owner state while constructing a
value, then expose a read-only surface.

Mutation is allowed when it is necessary for performance or platform
constraints, but the mutable phase, owner, and cleanup path should be explicit.

If a broad object needs many setters, consider splitting it:

- immutable snapshot/result
- mutable builder
- platform-local cache
- owner-managed live state

This avoids turning every object into a mutable bag of lifecycle hazards.

## 18. Patterns

### Put Shared Facts In The Parent

If two child nodes need the same value, the parent or an explicit shared owner
should usually hold it.

### Keep Local UI State Local

Disclosure state, temporary edit text, hover/focus state, and small local
choices should usually belong to the nearest meaningful Boundary or Node-local
state.

### Prefer Explicit Structure Over Hidden Traversal

If a child needs something, pass it through Props, a facade, or a direct borrowed
Boundary surface. Do not rely on hidden global lookup.

### Write Simply First

Start with clear ownership and readable state flow. Optimize after measurement
or after the lifecycle need becomes visible.

### Keep Allocation Failure Narrow

Loka does not use exceptions, so allocation-style failure needs a small,
predictable surface.

- Pointer-returning `clone()` / `create()` seams may return `0`, but only for
  allocation-style failure such as OOM.
- Contract misuse should be prevented structurally when possible, or stopped by
  debug `assert` rather than normalized as routine control flow.
- Owner-side assignment/setter code should stage replacement clones first and
  preserve the previous value when that staging fails.
- Reference-returning builder seams and constructor-only clone paths that cannot
  report failure explicitly should be treated as migration targets, not as the
  preferred pattern.

This contract defines the meaning of a nullable result; it does not claim that
every concrete allocator path can already produce one. Clone/create
implementations that still use plain `new`, or whose constructors allocate
internally, remain migration targets for an end-to-end no-exception OOM policy.

## 19. Framework Comparisons

Loka shares ideas with modern declarative UI frameworks, but it is not trying to
copy their runtime model.

Key differences:

- Loka does not rely on garbage collection.
- Loka does not rely on exceptions.
- Loka treats ownership as part of the app-facing model.
- Loka keeps platform projection separate from logical UI meaning.
- Loka is designed to scale down to old C++ and old operating systems.

The most useful mental model is not "this is the same as another framework."
It is:

```text
State facts + explicit ownership + Boundary-scoped composition + native projection
```

## 20. Rust / React Style Ownership Questions

From a React perspective, Loka can feel stricter because it does not encourage
arbitrary mutable state hidden behind closures or hooks.

From a Rust perspective, Loka does not use the Rust borrow checker, but it tries
to make ownership visible in the API shape:

- parent owns child-facing state by default
- shared state needs a meaningful owner
- mutable state should not be passed around casually
- Node-local state should not escape as a foreign mutation channel

`State<Managed<T>>` can be useful for shared payloads, but it is not a complete
ownership model. Use it when stable payload lifetime is the requirement, not as
a substitute for deciding who owns the resource.

## 21. First Instincts To Build

When writing Loka code, ask:

1. What is the application fact?
2. Who owns it?
3. Is it immutable, mutable, or an event?
4. Which Boundary observes it?
5. Which platform projection receives it?
6. Who cleans it up?

If the answer is unclear, the API or design is probably too vague.

## 22. What To Read Next

After this guide, read:

- `PHILOSOPHY.md`
- `docs/TODO.md`
- `docs/BoundaryMemoryDraft.md`
- `docs/MutabilityDraft.md`
- `docs/StateUpdateResultDraft.md`
- `docs/SceneProjectionTransactionDraft.md`

These documents contain the current design direction for ownership, memory,
mutation, update results, and projection scheduling.

## Summary

Loka is built around one application model:

```text
Write once.
One application model.
One declarative UI framework.
One ownership/composition philosophy.
Many eras and platforms.
```

The practical rule is simple:

```text
Make ownership visible.
Keep state scoped.
Prefer immutable completed values.
Use Boundary as the lifecycle/update unit.
Project logical UI into native platforms.
Avoid black boxes.
```

If application code reads naturally while ownership and lifecycle remain
inspectable, it is moving in the right direction for Loka.
