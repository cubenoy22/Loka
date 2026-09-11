# Loka Programming Guide

This guide tracks the current development source. For the guide as it stood
for a published release, read this file at that release's tag.

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
projection, and future resource management. The same vocabulary should explain
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
4. Only affected areas are recomposed, projected, laid out, or redrawn.

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

### `ObservableList` And `MirroredList`

[`ObservableList<T>`](../common/core/ObservableList.hpp) is a data-only model:
its single `revision()` State publishes a `ListRevision` containing `structure`,
`content`, and `change`. Changes are facts supplied by the data owner; the view
does not compare snapshots to infer a diff, and `change` summarizes the last
successful edit or apply, not a history. Each model item has a list-issued
`ItemId` (16-bit generation and 16-bit sequence), independent of its address or
current index.

`attach(tracker, capacity)` reserves the model's entry and scratch capacity;
the tracker must outlive attachment, and the attachment must outlive its lazy
view. Single edits or `apply(cursor)` publish under the model's tracker guard;
apply validates the complete replayable operation sequence before committing.
Check `ListAttachResult` and `ListEditResult`: allocation, capacity, identity,
index, exhaustion, attachment, and reentrant-edit refusals are explicit results,
and refused edits leave live entries and the revision unchanged.

[`MirroredList<T>`](../common/core/MirroredList.hpp) is a non-copyable working
copy for view-side editing that borrows the model, which must outlive it.
It reserves its working rows at construction and records pending insert/remove/
update/move operations; operation-log pages can still allocate and refuse.
`commit()` applies pending operations to the model, `cancel()` discards them,
and `undo()` removes the last pending operation only while the base revision
still matches; neither cancel nor undo reverses a completed commit.
Check `status()` and each `MirrorResult`, including its underlying model refusal;
a failed commit preserves pending work. Provisional IDs (generation 65535)
returned by the mirror's `insert()` belong to the mirror alone and expire at
commit or cancel: commit remaps only the pending operations internally, so a
provisional ID a caller kept is not turned into a model ID and a later model
edit with it returns `EDIT_ID_NOT_FOUND`. Read the model (or the next revision)
for the list-issued ID after a successful commit.

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
- a `Held<T>` view when the receiving scope explicitly holds a passive payload

`Held<T>` is not an anonymous smart pointer. Copying its handle does not retain
the payload. A `Boundary` or `Section` acquires an owner slot through
`NodeComposition::hold()`, and that slot is the lifetime edge. A scope outside
the creator's ownership subtree cannot acquire a slot, and a copied view must
not be used after its holding scope or creator landlord is gone. Values shared
across unrelated branches belong in a meaningful common owner, repository, or
immutable global cache instead.

`Held<T>` governs lifetime, not payload mutation authority. It does not make
`T` immutable; mutable operations still need an explicit owner, facade, or
State update path.

`Managed<T>` remains useful inside value plumbing such as String, Blob, and
Image payloads. It is not the normal app-facing answer for state or resource
ownership.

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

### `Match()`

The `onFailure` list is already a first-match-wins router: matchers are tried
in declaration order and the first hit handles the error. `Match()` gives that
structure a name for *values*, and lets an arm be either a value handler or a
whole child Flow that the parent awaits.

```cpp
using loka::dsl::Flow;
using loka::dsl::Match;
using loka::dsl::Step;

Flow()
  | Step(STEP_OPEN, OpenAdapter(...))                       // Out = OpenResult
  | Match<OpenResult, Image>(MATCH_OPEN)                    // In = previous Out
      .arm(&IsDecoded,      this, &TakeDecodedImage)        // value arm
      .arm(&NeedsRelease,   this, releaseAndRetryFlow)      // child Flow arm
      .otherwise(&ReportNoImage, this)                      // optional
  | Step(STEP_SHOW, ShowAdapter(...));                      // In = Image
```

- A matcher is `bool (*)(const In &, void *)`; arms are tried in declaration
  order and only the first hit runs. Matching is on the *value*: real failures
  still travel through `onFailure`, so a mismatch and a failure are never the
  same thing.
- A value arm is `StepRunStatus (*)(const In &, Out &, FlowError &, void *)`;
  it may return `FLOW_STEP_PENDING` like any adapter.
- A child Flow arm receives a `FlowChain<ChildIn, ChildOut>`. `ChildOut` must
  be exactly the Match's `Out` (a compile-time check). When `ChildIn` equals
  the Match's `In`, the matched value is fed to the child's first step;
  otherwise the child must supply its first step's `.input(...)` itself.
- `otherwise()` is optional. When no arm matches and there is no
  `otherwise()`, the run fails exactly like an unhandled step failure, so the
  flow-level `onFailure` list still applies.
- `Match` is an ordinary step: it shares the step id space, `resume(id)`, and
  the scenario audit column. While a child arm is pending the parent is
  pending, so the trigger drop rule already protects the child from re-entry;
  `cancel()` on the parent reaches the child and stops it at its next step
  boundary. Match adds no failure atomicity: an arm that ran halfway is not
  rolled back.
- The parent drives the child. External completion resumes the *parent* at
  the Match step id; the child is never resumed on its own.

## 10. Bidirectional Input And Update Loops

Bidirectional UI can accidentally create loops:

1. A control updates state.
2. State projection updates the control.
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
Projection carries the logical result into native controls.
```

### `Show()`

`Show()` should be understood as an attach/detach mechanism rather than merely
an `if` statement.

Retained attach/detach is the default: a hidden branch is parked, its nodes,
native contexts, and subtree-local state survive, and re-showing brings the
same identity back.

When the condition is a `State<bool>` itself, the `Show()` call can be
omitted: `isVisible << child` expands to exactly `Show(isVisible) << child`,
so both lines below build the same `ShowDefinition` — same retained
semantics, same `PolicyScope` composition, and further children keep
chaining with `<<` as usual. The shorthand applies only when the left
operand is a `State<bool>` reference.

```cpp
<< (Show(*this->detailsVisible_.state()) << this->detailsDefinition_)
<< (*this->detailsVisible_.state() << this->detailsDefinition_)
```

The other policy is explicit: `Show(condition).destroyOnDetach()` declares
that hiding the true arm destroys it. Re-showing constructs fresh descendants
whose local state starts from its initial values.

```cpp
<< (Show(*this->isDialogShown_.state()).destroyOnDetach()
    << OpenFileDialog().result(this->chooserResult_))
```

The policy belongs to that Show's true arm. Nested seats keep their own policy.
The deprecated `PolicyScope` annotation remains supported as the sole branch
root, including its existing `deliverWhileDetached()` behavior. The new Show
modifier adds no runtime node.

Modal nodes such as `OpenFileDialog` want the destroy side: the native dialog
dismisses itself on completion, so there is nothing worth keeping in the
logical subtree. Completion is delivered only through `result` / `onResult`,
and flipping the owning `Show()` condition back to false is the app's job — a
debug assert enforces that at least one completion binding exists.

The design goal is that memory and lifecycle are visible from the DSL structure.

### `LazyColumn()` / `LazyRow()`

See [LazyList](../example/LazyList/README.md) for a paged card view with content edits, structural edits, and compile-time capacity builds.

Use a lazy list for fixed-size component items backed by an `ObservableList`.
Each item Props type `T` names its `NodeType`, derived from `ComponentNodeWithProps<T>`.
The list and the viewport State belong to the app and must outlive the view.

```cpp
c.declare(LazyColumn(cards).cells(200, 20).viewport(*this->viewport_.state()));
```

`LazyRow(cards)` selects horizontal progression. `.wrap(count)` groups cells
across the other axis; `LazyFlex<CardProps>(cards).axis(STACK_AXIS_COLUMN)`
is the explicit form. The app writes the viewport rectangle in content coordinates.
An empty initial viewport creates no item controls until the first sized value.

Each item has a logical visibility seat using `Show(...).destroyOnDetach()`.
Visible items materialize their components and native controls; leaving the
viewport destroys those components, leaving no item native control or native
ledger row. The visibility seat survives, and a hidden item is built from its
current model value when it next appears.

A visible content edit uses `NodeDefinition::applyPropsToNode` and refreshes
`declareBindings` without re-declaration, preserving other item-local node state.
Inserting, removing, moving, or resetting items successfully replaces the entire
LazyScope generation: none of that generation's item-local state survives.
Put facts that must survive paging or structure changes in the model.
Scrolling out also ends the item's focus; LazyFlex does not restore focus when
it returns (native focus behavior still awaits runtime verification).

`LazyFlexNode::status()` reports `LAZY_FLEX_CAPACITY_REFUSED` when the attached
list's reserved capacity exceeds `LOKA_LAZYFLEX_MAX_ITEMS`, even if its current
size fits; a refused view declares no items. The default cap is 256; it is a
capacity contract over reserved entries, not a performance bound.
Viewport and list changes settle through the scene's normal queued update flush.

The cost of a viewport update (a page flip or scroll) grows superlinearly
with the number of declared items, not with the number visible: state
propagation, observed-state matching, and the visibility-seat walk each scale
with the item count. Content edits are different: `refreshContent` visits only
the changed range, and only a structure replacement is O(n). Measured on the LazyList
example (headless MAME, Macintosh IIx, 8 MB, one populated `Show` per item), a
steady page flip took about 0.43 s at 25 items, 0.97 s at 100, and 2.5 s at
200. In that configuration roughly 100 items keeps a page flip near one
second; larger lists on 68K hardware should expect that curve. A short, fixed
visible set whose values merely change is better served by plain State-driven
children than by a lazy list.

## 12. DSL And Composition

The normative app-facing conventions live in
[`API_STYLE.md`](API_STYLE.md). This section is their tutorial form.

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

### `For()` Or A Lazy List

Use [`For()`](../common/app/nodes/nestable/For.hpp) for a fixed set of items
whose declared children should all materialize: it expands once into owned
Section definitions when appended to its parent. Its `.window(first, count)`
selects a range at declaration time; it does not observe a moving viewport.
Use `LazyColumn()` / `LazyRow()` for an `ObservableList` whose native controls
should exist only within the viewport.

Think of a Ferris wheel: M gondolas are the logical item seats, the visible arc
holds native controls, and the queue is the model supplying item values.
Moving the window writes viewport State and flips visibility seats; it does
not recompose the Boundary. Replacing the whole structure uses a Keyed /
LazyScope generation replacement; the kernel has no recompose door.

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

### Fixed-Cell Layout

`Canvas` places ordinary composition children in fixed cells, in declaration
order. Its constant props select an axis, cell extents, and wrapping count;
its borrowed `State<Frame>*` viewport is the only live input. The viewport's
owner controls its content-space origin and size. Canvas owns no State and
performs no scrolling.

```cpp
Canvas(80, 20, this->viewport_.state()).wrap(3)
    << firstItem << secondItem << thirdItem;
```

`STACK_AXIS_COLUMN` (the default) fills columns before advancing down;
`STACK_AXIS_ROW` mirrors this by filling rows before advancing right.
Placement subtracts the viewport origin in int coordinates before narrowing
and adds the parent layout origin. The far-edge row is included even when it
just touches the viewport. Children outside the candidate rows are not laid
out; within those rows, the cross-axis intersection is also checked. This is
a placement container, not a native visibility or clipping owner. Previously
placed native children are not hidden or detached by changing the viewport.

The reported extent is the full content height, not the viewport height.
`CanvasNode::layoutStatus()` reports invalid inputs or coordinate range
refusals for the latest attempted layout; checks remain active in release
builds. The linked composition cursor costs O(first index) to reach the visible
range, then O(candidate children), with no full child measurement pass.

## 13. Events And Updates

Prefer `deferBind` for UI projection and lazy updates. Use `bind` only when
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
project changes into native controls.

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
- passive subtree resources can be held by named `Boundary` or `Section` scopes
- shared immutable resources can live in caches or repositories
- mutable resources expose a clear mutable phase and cleanup path

Avoid designs where anonymous reference counting becomes the only answer.
`Held<T>` records which owner scopes keep a passive payload alive, while handle
copies remain inert views. The last slot drop queues its releaser on the owning
clock instead of running observable cleanup from an arbitrary handle destructor.
Testing can render those facts as `held-by [section(...)]` rather than only an
unexplained count.

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

When using the low-level `WindowProps::scene(Scene *)` overload, the call adopts
the pointer: ownership transfers into the props handoff and then exactly once
into the Window. The caller must not delete or reuse the Scene afterward. The
Window owns its current Scene, keeps a detached Scene alive until that Window's
flush cycle closes, and reclaims all remaining current, queued, or retired Scenes
when the Window is destroyed. Prefer the definition overload in ordinary DSL
composition because it keeps this ownership transfer structural.

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

`Held<T>` is deliberately narrower than Rust's `Arc<T>`. It records owner scopes,
not alias count: copies do not retain, holds are limited to the creator subtree,
and the owning clock controls release. Mutable facts still belong to a meaningful
State owner; unrelated shared immutable payloads belong in repositories or
caches.

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
