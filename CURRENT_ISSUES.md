# Current Issues

## Scope

This note captures the current architectural direction after stabilizing retained `Show()` / `OpenFileDialog` behavior across macOS, Toolbox, and Win32.

The central shift is no longer "fix the dialog bug".
It is:

- keep persistent lifecycle facts on retained objects
- keep one-pass compose/apply facts in boundary-local temporary scope

The immediate target is the `Show()` / `Conditional` path.
The broader goal is to make the static-first model easier to reason about, easier to debug, and easier to extend toward homogeneous `ForEach<T>` later.

## What We Confirmed

### 1. `Show()` works better as retained attach/detach

For the current `0.0.1` scope, `Show()` behaves better when it:

- retains child identity
- detaches while hidden
- re-attaches the same subtree when shown again

This is more stable than destroy/recreate for common UI cases such as `OpenFileDialog`.

Practical result:

- repeated open/close no longer depends on fresh context creation
- subtree-local state survives hide/show
- attach/detach can become an explicit lifecycle instead of an accidental side effect of reconstruction

### 2. Native dialog delivery had a self-destruction hazard

The original `OpenFileDialog` crashes on macOS and Toolbox came from this pattern:

1. native callback enters context code
2. context writes state and/or emits an event
3. Flow / compose runs immediately
4. subtree or context may already be detached/destroyed
5. callback continues touching `this` or borrowed pointers

This is now mitigated by:

- copying borrowed pointers first
- avoiding use of `this` after notification begins
- guarding notification with lifetime tokens

### 3. Retained attach/detach still needs explicit phase

Retaining the subtree did not remove lifecycle state.
It only clarified which lifecycle state is real.

Retained dialog/context objects still need to know:

- should present on next attach?
- currently presenting?
- already presented during this attach interval?

This is why `OpenFileDialog` now uses a small value-object phase model instead of ad hoc booleans.

### 4. The remaining weakness is not dialog-specific

The dialog bug exposed a more general issue:

- persistent lifecycle state is mostly understandable now
- one-pass compose/apply facts are still scattered

The most visible current example is `composeAttachState_` / `consumeComposeAttachState()`.

## Core Design Split

### A. Persistent facts belong to retained objects

Examples:

- attached vs detached lifecycle
- dialog presentation phase
- retained native context ownership

These survive across ticks and hide/show cycles.
They belong on retained node/context-local objects.

Current example:

- `OpenFileDialogPresentationPhase`

Good properties:

- small value object
- explicit transition methods
- no dependency on one-pass traversal timing

### B. One-pass facts belong to a boundary-local temporary scope

Examples:

- this child became active during the current pass
- this child should compose as `ATTACH` even if the parent pass is `UPDATE`
- this dispatch hint is valid only during the current compose/apply cycle

These do **not** belong on `Node`.
They are not persistent lifecycle.
They are temporary interpretation results for one pass.

They should live in:

- a boundary-local compose/apply working scope
- a local traversal/disposition context
- a temporary compare/apply result

and be discarded after the pass ends.

## Why Dynamic Already Feels Closer To Correct

### Dynamic composition already has local truth ownership

Dynamic paths already lean on boundary-local ownership of truth:

- previous snapshot
- current snapshot
- diff result
- local rebuild plan

That means facts such as:

- retain
- replace
- retire
- attach

are derived from explicit boundary-local comparison.

This is the important part.
The strength is not "it has a fancy diff".
The strength is:

- truth is local to the boundary pass
- truth is derived rather than stored on child nodes
- apply decisions are made by the owner

### Static / `Show()` / `Conditional` currently lack an equivalent local phase

In the simpler `Show()` / `Conditional` path, the framework still needs to answer:

- is this child fresh for this pass?
- should this child compose as `ATTACH`?
- is this a retained subtree re-attach rather than an ordinary update?

Today, that answer is approximated with node-local one-shot state:

- `markPendingAttachForCompose()`
- `composeAttachState_`
- `consumeComposeAttachState()`

This is the current mismatch.

In short:

- Dynamic derives pass truth from boundary-local compare/apply state
- Static `Show()` currently fakes pass truth with a temporary child-node flag

## Main Remaining Concern

### `consumeComposeAttachState()` is still the clearest smell

The existing API exists because the original need was real:

- `ConditionalNode` knows a fresh child became active
- `Boundary` chooses which compose event to send
- the information must travel somehow

But the current shape is still weak.

A `consume` API combines:

- read
- decision
- clear

That can be fine for queue-like work items.
It is much less convincing for lifecycle/dispatch truth.

Concerns:

- who is allowed to consume it?
- can it be consumed too early?
- is the decision still visible when another layer needs it?
- why does a child node own a fact that only matters during its parent boundary pass?

This is the most immediate architectural target.

## Current Direction

### 1. Introduce a reduced boundary-local apply phase for static paths

The next likely direction is **not** a heavyweight general transaction framework first.

The next step is a reduced boundary-local compose/apply phase for static paths:

- compose builds the current structure
- the boundary derives temporary child disposition for this pass
- traversal/apply uses that disposition
- the disposition is discarded after the pass

This should be understood as a reduced sibling of dynamic diff/apply:

- Dynamic: richer compare + apply
- Static `Show()` / `Conditional`: reduced apply/disposition only

The purpose is to recover local truth ownership without turning static composition into full dynamic diff.

### 2. Keep the first scope very small

First-pass target:

- fresh child attach upgrade
- retained subtree re-attach interpretation
- explicit child compose dispatch for `Show()` / `Conditional`

Not in the first scope:

- generic loop/list reconciliation
- heterogeneous child reuse
- move/reorder optimization
- global async transaction redesign
- heavyweight scene-wide queue framework

### 3. Treat native/UI hosts as delayed mirrors, not as truth

The logical UI should remain the single source of truth.
That does **not** mean the host side is always synchronized immediately.

The intended model is:

- logical UI owns the current truth
- projection/apply may be delayed and coalesced
- host/native/UI is an observed mirror of that truth
- older host results must never overwrite newer logical truth

This matters even for local native UI.
Dialogs, timers, deferred callbacks, and retained controls already behave like a delayed host.
In other words, the architecture should be friendly not only to local retained UI, but also to future SSR-like / remote-host use cases.

The important rule is not "always fully synchronized".
The important rule is:

- the logical side stays self-consistent
- delayed host results are accepted only if they still belong to the current logical request/lifecycle
- stale results can be discarded safely

### 4. `nextTick` already acts as a scheduling queue

Current `nextTick` behavior already provides an important part of the delayed model:

- multiple logical updates are coalesced
- host apply is deferred to a known flush point
- latest logical truth wins within the tick

This means the immediate need is **not** a huge new queue subsystem.
`nextTick` is already the scheduling layer.

What is still missing is a clearer representation of the **content** of one update wave:

- which targets became dirty
- which boundaries became update roots
- which structure/layout/paint facts belong to this wave
- which host request/result still belongs to the current logical generation

That is the role later concepts such as:

- `ProjectionTransaction`
- `ObservationTransaction`
- request/result generation tokens

### 5. Future-friendly direction: logical truth -> projection -> observed host

The direction that now seems most stable is a three-layer model:

1. Logical truth
   - tree/state/ownership/lifecycle
   - authoritative
2. Projection / observation transaction
   - one update wave worth of derived facts
   - coalesced, optimizable, discardable after apply
3. Observed host
   - native UI / OS / browser / remote target
   - delayed mirror, never the source of truth

This framing should make future extensions easier:

- local native UI
- classic retained host APIs
- remote or SSR-like host delivery
- stale callback/result rejection

The key architectural benefit is not "network support".
It is that delayed or out-of-date host state becomes an expected part of the model instead of a bug-shaped surprise.

### 6. Prefer disposition / plan naming over overloaded "diff"

There are two related but different concepts:

- true previous/current structural comparison
- one-pass dispatch meaning derived for the current compose/apply pass

So naming should likely stay explicit:

- `compare...` / `build...Diff` for actual previous/current comparison
- `derive...Disposition` / `build...Plan` for current-pass dispatch/apply interpretation

The old instinct that `NodeComposition - NodeComposition` was the wrong shape was probably correct.

## Temporary Model To Aim For

The practical target model now looks like this:

### Persistent memory

Owned by the boundary or retained objects.

Examples:

- previous composition snapshot
- current composition snapshot
- retained context lifecycle phase

### Temporary result

Owned by the boundary-local working scope.

Examples:

- attach / retain / retire interpretation for this pass
- projection targets collected during this update wave
- structure/layout/paint facts derived for this flush
- generation/request facts needed to reject stale host results

Examples:

- diff result
- compose disposition
- local rebuild/apply plan
- attach-upgrade hints

### Node-local one-shot flags

Should be reduced or removed.

Current suspect:

- `composeAttachState_`

## `ForEach<T>` / Loop Direction

Loop/repeat support should be considered in the design now, even if implementation comes later.

However, the intended first shape is intentionally constrained:

- homogeneous children only
- effectively `ForEach<T>` rather than a heterogeneous mixed-type repeater
- no generic shared reuse pool in v1
- no advanced move/reorder optimization in v1

This means the first useful repeated-child semantics can stay relatively simple:

- attach
- retain
- retire

with optional later growth toward:

- replace
- move
- richer reuse policy

This matters because it means the same boundary-local apply/disposition model can cover:

- `Show()` as `0/1` child disposition
- `Conditional` as branch disposition
- `ForEach<T>` as homogeneous repeated-child disposition

without requiring a full generic reconciler immediately.

## Why This Should Stay Boundary-Local

Another way to view the issue:

- `StateTracker` detects changes
- dirty is reported
- compose/apply eventually reaches `PlatformController`

The current weakness is that the "one update pass worth of interpretation" is not explicit enough in the middle.

That is why facts leak into:

- node-local one-shot flags
- platform comments that compensate for already-consumed attach hints

The likely owner of the missing local unit is still `Boundary`, not `Node`, and not a scene-global object first.

That matches existing dynamic/local-diff direction:

- boundary-local diff/apply work already exists
- what is missing is cleaner reuse of that idea for static structural cases

## Projection Direction

At this stage, the important thing is **not** to fully design a universal command framework yet.
The more useful missing word is probably `Projection`.

Current leaning:

- Loka's primary job is to build logical UI
- OS / native / browser / remote backends should receive a projection of that logical result
- platform side should receive interpreted results rather than re-reading live node-local one-shot state

This makes a projection layer attractive because it gives:

- a stable pass-local payload
- better separation between logical interpretation and platform execution
- easier batching and later optimization
- much better testability through a virtual OS / test executor
- a natural place to support remote / SSR-like transport later

### Projection layering

The current direction is starting to look like three levels:

- `BoundaryProjection`
- `SceneProjection`
- platform execution payload / command stream

Meaning:

- `Boundary` derives local projection from its own compose/apply result
- `Scene` gathers and combines multiple boundary-local projections
- `PlatformController` executes the final scene-level projection

This is important because nested boundaries can still produce updates that affect a wider scope:

- a button inside a child boundary may change state owned by an outer boundary
- sibling subtrees may need relayout/repaint
- multiple local projections may need to be combined before platform apply

So projection cannot stay purely boundary-local forever.
The likely model is:

- local truth stays boundary-local
- combined projection becomes scene-level

### Relationship to commands

Commands are still a useful likely implementation technique.
But they now look more like an internal representation of projection rather than the primary architectural word.

Current leaning:

- logical UI computes absolute intended result
- boundary-local phase derives temporary disposition / projection
- scene combines local projections into `SceneProjection`
- optional command/optimizer layer may later package that result for execution
- platform executes the packaged projection

This keeps delta/diff where it belongs:

- not in logical UI truth itself
- acceptable later for transport, batching, optimization, delay compensation, or execute-time coalescing

This also fits future possibilities such as:

- virtual platform testing
- remote/SSR-like transport
- game/sprite-oriented backends

without forcing logical UI itself to become diff-oriented.

### Why `Scene` becomes more important

This direction also gives `Scene` a clearer role.

Instead of acting mostly as a scheduler/invalidation waypoint, `Scene` starts to look like:

- the owner of cross-boundary projection gathering
- the owner of scene-level optimization/combination
- the natural place where local boundary results become a scene-level applyable unit

This may also help absorb previously floating responsibilities around `SceneManager` / scene switching.

A scene switch is also projection-shaped:

- detach/retire old scene
- attach/initialize new scene
- rebuild relayout/redraw consequences

So scene switching, redraw aggregation, and cross-boundary update combination may all become easier to reason about under one projection model.

## Immediate Working Goal

The next branch should probably focus on this exact slice:

- remove or reduce node-local `composeAttachState_`
- replace it with boundary-local temporary compose/apply disposition
- prove the path first on `Show()` / `Conditional`
- keep `ForEach<T>` as a design target, not the first implementation payload

If that succeeds, the framework gets:

- clearer ownership of pass-local truth
- less fragile lifecycle/debug behavior
- a more natural path toward homogeneous repeat/collection DSL

## Non-Goals For This Step

To keep momentum and avoid premature abstraction, this step should **not** try to solve all of the following at once:

- full dynamic/static unification
- scene-global transaction framework
- arbitrary heterogeneous reuse
- generic keyed tree diff for all node shapes
- advanced move/reorder animation substrate
- platform scheduling redesign beyond what is needed for the local compose/apply boundary

## Open Questions

### 1. What is the smallest useful reduced apply-phase type?

The next implementation still needs a concrete local shape.

Open choices:

- a traversal-local context passed explicitly
- a boundary-owned temporary working object
- a small disposition table keyed by child identity

### 2. How should `Conditional` / `Show()` register temporary attach truth?

Today the information is placed on the child node.
The replacement is not chosen yet.

Open choices:

- direct registration into a boundary-local working scope
- parent-side derivation from structure changes
- temporary composition-state extension

### 3. How much should the first step know about future `ForEach<T>`?

`ForEach<T>` is intentionally constrained in v1:

- homogeneous children only
- simple retain/attach/retire semantics first

But it is still open how much of that future shape should influence the first static reduced apply model.

### 4. How should `BoundaryProjection`, `SceneProjection`, and command payload be separated?

There are likely at least three useful layers now:

- boundary-local interpretation / projection
- scene-level combined projection
- platform-facing execution payload / command stream

The naming and ownership boundary between them is still open.

### 5. How much live `Node` access should platform apply still allow?

If `NextTickTracker` or later deferred execution reorders apply timing, re-reading live nodes can become semantically unsafe.

Open question:

- should the platform side eventually consume only stable payload/commands for some paths?
- or is limited live-node access still acceptable for simple immediate cases?

### 6. Where should projection/command optimization live?

The architecture likely benefits from eventual batching/coalescing/skip behavior.
But that should not pull diff logic up into logical UI.

Open question:

- should optimization sit between `BoundaryProjection` and `SceneProjection`?
- between `SceneProjection` and platform execution?
- or stay platform-specific for longer?

### 7. Which commands are latest-wins vs must-execute?

For future command/plan design, not every action has the same semantics.

Examples that may tolerate coalescing:

- visible/text/frame/property updates
- some redraw/invalidate cases

Examples that are more likely must-execute:

- attach/detach transitions
- resource acquire/release
- one-shot dialog/open/close type operations

The exact classification is still open and should not be overdesigned too early.
