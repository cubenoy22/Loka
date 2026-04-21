# Current Issues

## Scope

This note records the current architectural state after the retained `Show()` /
`OpenFileDialog` fixes and the first Scene/Projection transaction lifecycle
cleanup.

For `0.0.1`, the important result is that the original dialog and retained
visibility problems are no longer the main risk. The remaining work is mostly
about continuing to make update ownership, temporary facts, and platform
projection easier to reason about.

## Current State

### Retained `Show()` uses attach/detach semantics

For the current `0.0.1` scope, `Show()` is treated as a non-loop retained
visibility primitive:

- child identity is retained while hidden
- hidden children detach from the active projection
- showing again re-attaches the retained subtree

This avoids the fragile destroy/recreate behavior that made dialog and callback
lifetime hard to reason about. Loop/list reuse remains out of scope until a
dedicated `For` / `ForEach<T>` design lands.

### Native dialog callbacks are lifecycle-aware

The original `OpenFileDialog` crash shape was:

1. native callback enters context code
2. context writes state and/or emits an event
3. Flow / compose runs immediately
4. subtree or context may detach/destroy
5. callback continues touching stale `this` or borrowed pointers

The current implementation avoids that class of bug by:

- keeping dialog presentation state in a small phase object
- copying borrowed pointers before notification
- avoiding use of context-owned state after notification starts
- guarding result delivery against stale lifecycle

The broader rule is that native callbacks should never assume the logical tree
is unchanged after a state write or event emission.

### Compose attach state is encapsulated

The old smell was a scattered `pendingAttach` / `consume...` style flow where
callers had to know when a temporary attach hint should be read and cleared.

The current shape is better:

- `ComposeAttachLifecycle` owns the attach hint transition
- callers resolve child compose events through a narrow API
- platform code does not see attach hints
- tests cover the attach lifecycle behavior

This is good enough for `0.0.1`. It is still not the final ideal, because the
temporary pass fact is still node-local. A future pass can move more of this
truth into a boundary-local working scope.

### Scene update facts are grouped into snapshots/transactions

The first Scene/Projection lifecycle cleanup introduced a clearer split:

- `SceneProjectionTransaction` collects dirty projection targets for one update
  wave.
- `SceneUpdateRequestSnapshot` captures requested dirty flags, transaction
  dirty flags, update roots, and effective rebuild requirements.
- `SceneUpdateApplySnapshot` captures structure/layout/paint analysis.
- `SceneUpdateSnapshot` ties request/apply facts to a generation.
- normal app-facing APIs no longer expose transaction internals directly.
- test-only introspection goes through `SceneTestAccess`.

This does not yet implement a full command/projection framework. It does
establish the local structure needed to keep future changes from becoming more
flag-driven.

## Design Rules Confirmed

### Persistent facts belong to retained owners

Examples:

- attached vs detached lifecycle
- dialog presentation phase
- retained native context ownership
- scene-level pending update state

These can cross event cycles, so they need an explicit owner such as `Scene`,
`SceneDirector`, `Boundary`, a retained `Node`, or a retained platform context.

### One-pass facts should stay local

Examples:

- this child should compose as `ATTACH` during this pass
- these targets became dirty in this update wave
- this layout/paint/structure classification belongs to this flush
- this host result belongs to this request generation

These should remain in snapshots, transactions, local working scopes, or other
obviously one-shot structures. They should not become scattered long-lived
flags unless there is a clear owner and lifecycle.

### Logical UI remains the source of truth

Native UI, Classic Toolbox controls, Cocoa views, Win32 HWNDs, timers, dialogs,
and future remote/SSR-like hosts are all delayed mirrors.

The model should assume:

- logical UI owns truth
- projection/apply may be delayed and coalesced
- stale native results must be safely ignored
- platform apply should receive interpreted update facts, not re-decide
  logical ownership/lifecycle policy

## Why This Matters

The hard bug was not only a dialog bug. It exposed a broader class of failures:

- callbacks firing after the logical tree changed
- one-shot lifecycle flags being consumed at unclear times
- dirty/update facts spread across `Node`, `Boundary`, `Scene`, and platform code
- platform code compensating for logical state it should not need to know

The new direction reduces those risks by making data lifetime visible:

- retained state is owned by retained objects
- update-wave facts are grouped into snapshots/transactions
- test-only inspection does not widen production APIs
- platform code stays closer to projection execution

## Remaining Work

### 1. Boundary-local temporary compose/apply scope

`ComposeAttachLifecycle` is acceptable for now, but the cleaner long-term shape
is still boundary-local:

- `Conditional` / `Show()` derive child disposition during the owner boundary
  pass
- traversal uses the resolved event/disposition
- one-pass facts are discarded with the pass

This is the likely bridge toward future homogeneous `ForEach<T>` support.

### 2. SceneManager alignment

`SceneManager` still predates the newer Scene/Projection transaction model.
It should be revisited separately after this branch lands.

The goal is not to broaden this refactor, but to align scene switching,
pending transition ownership, and cancellation with the newer lifecycle model.

### 3. Platform controller layout duplication

macOS, Win32, and Toolbox layout traversal still share too much duplicated
logic. This is not a `0.0.1` blocker, but it remains a high-value cleanup
because behavior fixes currently need manual porting across platforms.

### 4. Future projection/command model

Commands are still a useful possible implementation technique, but the primary
architectural word should remain `Projection`.

Likely layering:

- boundary-local interpretation
- scene-level combined projection
- platform-facing execution payload or command stream

This should stay future work until the current transaction/snapshot shape has
more real usage.

### 5. Loop/list semantics

`ForEach<T>` should be designed as a constrained homogeneous repeated-child
primitive first:

- attach
- retain
- retire

Advanced heterogeneous reuse, keyed reconciliation, move/reorder, and shared
reuse pools are not `0.0.1` goals.

## `0.0.1` Release Readiness View

The core architectural risk that blocked confidence in `Show()` / dialog
behavior has been reduced enough for `0.0.1`.

Before release, prefer sanity checks over large refactors:

- run Tutorial and examples as real user flows
- verify macOS / Win32 / Toolbox samples where available
- review README and quick-start paths
- keep remaining architecture work in `docs/TODO.md`
- avoid pulling future `ForEach`, command, or SceneManager redesign work into
  the release branch unless a concrete bug requires it
