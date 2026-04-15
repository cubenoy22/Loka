# Current Issues

## Scope

This note summarizes the current architectural issues and newly clarified direction after stabilizing `OpenFileDialog` across macOS, Toolbox, and Win32.

The understanding has shifted from "one bad dialog bug" to a broader distinction between:

- persistent lifecycle state that belongs to retained nodes/contexts
- one-pass metadata that belongs only to a single update/compose transaction

## What We Confirmed

### 1. `Show()` behaves better as retained attach/detach

For the `0.0.1` scope, `Show()` now makes more sense as:

- keep the child identity
- detach it while hidden
- re-attach the same child when shown again

This is simpler and more stable than destroy/recreate for common UI cases such as `OpenFileDialog`.

This also means:

- repeated open/close is no longer forced to rely on fresh context creation
- subtree-local state survives hide/show as expected
- the system can use explicit attach/detach phase transitions instead of reconstructing identity from scratch

### 2. Native dialog delivery had a self-destruction hazard

The original `OpenFileDialog` crashes on macOS and Toolbox were caused by this pattern:

1. native dialog callback enters context code
2. context calls `MutableState::set()` and/or `EmitterState::emit()`
3. Flow / compose immediately runs
4. the subtree or context may already be detached/destroyed
5. callback code continues touching `this` or borrowed pointers

This was fixed by making result delivery self-destruction safe:

- copy borrowed state/emitter pointers first
- stop using `this` before notification
- guard event delivery with lifetime tokens

### 3. Retained attach/detach still needs explicit phase

Retaining the subtree did not eliminate lifecycle state.
It only changed the kind of lifecycle state that matters.

Once `OpenFileDialogContext` survives hide/show, it still needs to know:

- whether it should present on next attach
- whether it is currently presenting
- whether it has already presented during the current attach interval

So attach/detach-only `Show()` reduces ownership hazards, but explicit phase handling is still required.

This is why `OpenFileDialog` now has a small presentation phase object instead of ad hoc bools.

## Current Direction

## A. Persistent lifecycle state stays on retained objects

Examples:

- whether a retained dialog should present on attach
- whether a dialog is currently presenting
- whether a retained native context is attached or detached

These belong on node/context-local state because they survive across ticks and attach/detach cycles.

Current example:

- `OpenFileDialogPresentationPhase`

This is a good fit for:

- small enum-like value object
- no heap allocation required
- transition methods such as `beginPresent()`, `markPresented()`, `markDetached()`

### B. One-pass compose/update facts should not live on nodes

Examples:

- this child was freshly inserted during the current update
- this child should receive `ATTACH` even though the parent event is `UPDATE`
- this dispatch hint is valid only during this compose/apply pass

These facts are transaction-local, not persistent lifecycle state.

They should ideally be stored in:

- a stack-local compose/update transaction object
- a local dispatch context
- a local resolver/functor

and discarded at the end of the pass

This becomes more important as deferred work (`NSTimer`, message queue, later async delivery) becomes normal.
If one-pass metadata is stored on nodes, it can leak across passes and become inconsistent.

## Why The Earlier Weakness Showed Up

### Dynamic composition had diff-based truth

Dynamic composition already has a stronger model:

- previous composition
- current composition
- comparison/diff result

That means facts such as:

- inserted
- reused
- retired

can be derived from explicit comparison.

### Std/Show paths did not have equivalent local truth

In the simpler `Show()` / `Conditional` path, the system did not carry the same explicit previous/current comparison data.

So when a parent was in `UPDATE` but a child was actually fresh, the implementation needed an extra signal to say:

- "this child should be treated as `ATTACH`"

That signal became `pendingAttach`, then `ComposeAttachState`, but the deeper weakness was:

- one-pass dispatch truth was being approximated with node-local one-shot state

In short:

- Dynamic composition could derive truth from diff
- Std/Show paths were faking that truth with a temporary node flag

## Main Remaining Concern

### `consume*()` is still a smell for lifecycle/dispatch metadata

`consumeComposeAttachState()` exists because the original need was:

- `ConditionalNode` knows a fresh child was inserted
- `Boundary` decides which compose event to send
- the information was passed as a one-shot signal

This background is understandable, but the API shape is still weak.

A `consume` API bundles together:

- reading state
- deciding based on state
- clearing state

That is natural for queue-like event items, but less natural for lifecycle/dispatch metadata.

The concern is:

- who is allowed to consume it
- whether it gets consumed too early
- whether the state disappears before a full decision is made

Longer-term, this should move toward:

- explicit state objects
- `shouldXxx()` / `beginXxx()` / `markHandled()` style transitions
- or, better, pass-local dispatch context instead of node-local one-shot consumption

## Node / Context Attach-Detach Hooks

Attaching and detaching retained subtrees required explicit hooks.

Current minimal mechanism:

- `NodeContext::onNodeAttached()`
- `NodeContext::onNodeDetached()`

This turned out to be important for retained native nodes, because fresh context creation is no longer the only way lifecycle code can run.

Also, it was necessary to notify recursively through the retained subtree.
Notifying only the branch root was insufficient when `OpenFileDialog` sat under `Fragment` or another subtree wrapper.

This hook mechanism is useful, but still somewhat draft-like:

- recursive attach/detach notification currently lives in `Conditional.cpp`
- the contract is still minimal
- other retained subtree cases may eventually want a more central traversal helper

## Risks That Are Lower Now

These got much better after the recent changes:

- repeated open/close on all 3 platforms
- direct dependency on fresh context creation for dialog presentation
- owner/context dangling after immediate dialog result delivery
- `Show()` destroying simple subtree-local state by default

## Risks That Still Matter

### 1. Transaction-local metadata is not fully modeled yet

Persistent phase is now modeled better than before.
Per-pass dispatch metadata is not.

This likely matters later for:

- loop/list DSL
- reuse semantics
- reorder/move handling
- more advanced partial diff/apply paths

### 2. `Conditional` and `Show()` are still sharing implementation

Current retain attach/detach behavior was introduced through `ConditionalNode`.

That works, but it is not yet clear whether:

- all `Conditional(trueDef, falseDef)` cases should share the same retained behavior
- or `Show()` should eventually get a more explicit/specialized path

For now, the implementation is acceptable for the current scope, but it still feels closer to a validated draft than a final abstraction.

### 3. Deferred work and pass boundaries need a clearer model

The more the framework relies on:

- `NSTimer`
- posted messages
- deferred relayout/apply
- async callbacks

the more dangerous it becomes to store one-pass metadata on persistent objects.

This is the strongest argument for a future compose/update transaction object.

## Practical Design Split

This is the current best mental model:

### Persistent facts

Belong to retained nodes/contexts.

Examples:

- attached/detached phase
- dialog presentation phase
- retained native context ownership

### One-pass facts

Belong to the current update/compose transaction only.

Examples:

- fresh child insertion during this pass
- attach-upgrade dispatch hints
- local dispatch decisions derived during traversal

## Suggested Near-Term Follow-Up

### High priority

- Keep `Show()` simple: retained attach/detach first, no loop semantics in `0.0.1`
- Continue using small explicit phase objects instead of unrelated bool fields
- Avoid fresh-create-only assumptions in retained native contexts

### Medium priority

- Replace remaining `consume`-style lifecycle metadata with clearer transition APIs
- Explore a stack-local compose/update dispatch context for one-pass metadata
- Decide whether `Show()` should eventually diverge from generic `Conditional`

### Lower priority

- Expand attach/detach hook contract if more retained native nodes need it
- Add targeted tests for transition/state-object behavior directly
- Revisit UI callback lifetime pruning via `bindForUi()` once the transaction model is clearer

## Current `0.0.1` Position

For `0.0.1`, the intended scope is:

- `Show()` is not a loop/list reuse primitive
- `Show()` should behave as retained attach/detach by default
- loop/list reuse semantics remain future work
- shared reuse pool / lazy reuse remain future work

This keeps the initial model simple while avoiding the most fragile destroy/recreate behavior.
