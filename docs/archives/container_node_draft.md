# ContainerNode Draft

## Status

- Status: draft only
- No `ContainerNode` layer is implemented yet.
- This file is still useful as design direction, but should not be read as current architecture.

Current related confirmed direction:

- `Boundary` remains the ownership unit
- `Scene` should trust the root boundary only for local-diff downgrade decisions
- subtree-recursive aggregate downgrade is too coarse and was intentionally avoided

That means any future `ContainerNode` work must fit under boundary ownership rather than reintroducing scene-wide recursive policy.

## Goal

Define a lighter-weight child-diff layer that sits below `Boundary` ownership without importing a full generic reconciler.

The immediate goal is to support localized child replacement without resetting unrelated sibling controls in the same visual region.

## Problem Statement

Current `DynamicComposition` behavior is correct but coarse:

- `NODE_DIRTY_PROPS` and `NODE_DIRTY_LAYOUT` preserve native context identity.
- `NODE_DIRTY_CHILD` rebuilds the whole dynamic boundary subtree.
- As a result, branch swapping works, but sibling native controls in the same dynamic boundary are not yet guaranteed to preserve context or value identity.

This limitation already appears in three places:

1. Dynamic branch swap with sibling `EditText` / `PopupMenu`
2. Reactive menu rebuild granularity
3. Future `for` / list-style containers such as `LazyColumn`

## Arena Ownership Conflict

This is not a secondary implementation detail.
It is one of the core blockers for any real `ContainerNode` work.

Current `DynamicComposition` behavior on `NODE_DIRTY_CHILD` is effectively boundary-wide:

- the dynamic boundary clears its arena-owned subtree
- then rebuilds the child structure from scratch

That means a local container cannot preserve siblings merely by "wanting to retain them".
If the arena is cleared boundary-wide, retained children still disappear.

So any real `ContainerNode` design must answer this early:

- how can some children survive local child replacement
- while other children are rebuilt
- without breaking the current boundary ownership model

### First candidate direction: retain / retire / sweep

The most promising first direction is a localized retained-child mechanism rather than a fully separate ownership model.

Conceptually:

1. mark children that should survive this local replacement
2. retire children that are being removed, but do not destroy them immediately
3. sweep retired children at a boundary-safe teardown point

This is similar to a very small, explicit mark-and-sweep pass, but only for retained child structure inside the owning boundary.

The important point is scope:

- not a general GC system
- not a whole-program memory model
- only a retained-child lifecycle mechanism inside boundary-owned composition

### Grace-period retirement

Immediate destruction is not always the best policy.

For many UI cases, a removed child may return shortly:

- `showIf` toggles
- menu branch visibility changes
- temporary game/UI overlays

That suggests a grace-period retirement model:

- retired children are marked as removal candidates
- they are not necessarily destroyed immediately
- a later sweep decides whether they are truly released

Possible sweep triggers:

- after N ticks
- after a time budget such as ~1 second
- on explicit memory pressure
- during an idle/low-priority maintenance point

Current leaning:

- the owning `Boundary` should be the primary sweep trigger owner
- useful trigger points are composition-end, flush-end, and explicit idle/maintenance opportunities
- broader scene/window-level idle hooks may still help on constrained targets, but ownership should remain boundary-led

This gives the design a useful middle ground:

- deterministic enough to reason about
- still able to reuse recent children/native contexts
- does not require immediate full persistent ownership

### Transitional reuse direction

There are two related but distinct retention layers:

1. retain node/subtree structure
2. recreate node structure but retain native context for selected controls

The second option may be a useful transitional step for controls such as:

- `EditText`
- `PopupMenu`

It can improve visible UX retention even before a fuller retained-child structure model is complete.

However, this must be treated carefully because node ownership and native context ownership can otherwise drift apart.

Current leaning:

- primary long-term direction: retained child/subtree support
- acceptable transitional direction: grace-period retired native-context reuse for selected controls

### Secondary candidate direction: persistent side ownership

Another possible direction is to let some container-managed children live outside the main dynamic arena clear path.

That may work for some cases, but it introduces a second ownership lane and therefore increases lifecycle complexity.

Current leaning:

- prefer retain / retire / sweep inside the existing boundary ownership model first
- treat persistent side ownership as a fallback option, not the default design

## Current Contract

`Boundary` currently owns the following responsibilities:

- state ownership
- observed-state registration and dirty routing
- composition lifecycle
- scene/platform synchronization boundary

This contract is still valid and should remain the outer ownership model.

Ownership rule of thumb:

- follow gravity
- parent-owned is the default
- cross-boundary sharing must be explicit
- avoid designs where a child effectively stabilizes or owns its parent

In practical terms, shared data should usually be one of:

1. parent-owned
2. explicitly managed shared ownership (`Managed<T>` or equivalent)
3. global cache for immutable/shared resources

## Boundary Opacity And Freeze Semantics

Boundary-local optimization only works if boundaries remain real containment units.

Current leaning:

- a boundary may itself be child-holding
- but one boundary should not reach into the internal children of another boundary
- a child boundary is treated as an opaque retained child from the parent boundary's point of view

This is important for both correctness and performance:

- lifecycle ownership stays local
- diff/apply scope stays local
- nested boundaries do not accidentally trigger uncontrolled structural walks

### Why opacity matters

Without opacity, a parent-side optimization can easily expand into:

- cross-boundary child probing
- cascading local diff
- unclear ownership during retain/retire/sweep

That would quickly destroy the "small local algorithm" property.

### Freeze direction

Boundary opacity also leaves room for an explicit freeze model later.

Conceptually, a frozen boundary would:

- stop structural propagation at that boundary edge
- prevent unexpected cascading work across nested boundary trees
- allow the runtime to trade flexibility for predictable performance

This can be useful in game-style workloads as well as ordinary UI:

- expensive nested overlays
- card/deck UIs
- temporarily stable subtrees
- deliberate frame-budget protection

The first implementation does not need to introduce a full freeze API.
However, the design should leave room for boundary-level freezing as a future optimization control.

## Proposed Direction

Introduce an opt-in `ContainerNode` layer with a narrower role:

- child access owner
- local child identity metadata owner
- local child access/cache owner
- optional child-slot lookup owner

`ContainerNode` should not become a second `Boundary`.
It should not own the full lifecycle/state model by default.

The intended split is:

- `Boundary`: lifecycle/state/dirty owner
- `ContainerNode`: local child structure owner
- leaf/control nodes: native reflection owner

Current design preference:

- `ContainerNode` should be a real `Node` subclass
- it should participate in the normal node tree directly
- type-specific access should follow the existing Loka pattern (`asXxx()` / `NodeKind` checks), not RTTI

This keeps the model consistent with the current scene architecture and avoids introducing a detached helper object that mirrors part of the node tree.

It also gives a practical DSL benefit:

- if standard child-holding nodes such as `VStack`, `HStack`, `Grid`, or `ZStack` eventually become `ContainerNode`-based
- then `showIf` / `for` / local branch replacement can target the usual parent containers directly
- users do not have to learn a separate "special parent" rule for most composition cases

## Why Not A Full React-Style Reconciler

A full generic reconciler would likely push the project toward:

- heavier key bookkeeping
- broader child-array management
- more hidden allocations
- more complex update reasoning

That is a poor fit for the current Loka goals, especially for Classic/68k targets.

The target is not "full React inside every boundary".
The target is "small, explicit local child replacement where it matters".

Another way to frame it:

- a full reconciler solves this problem across the whole tree
- `ContainerNode` aims to solve the same class of problem locally, with simpler and more explicit matching data

If a container can keep enough intermediate child identity information, it can perform a deliberately simple retain/replace pass without adopting a fully generic tree reconciler.

## 68k Constraints

Any design in this area must stay compatible with low-memory and low-CPU paths.

Preferred constraints:

- no mandatory `std::vector`-heavy child diff path
- no hash-map-based key reconciliation in the common path
- no hidden heap churn for routine child replacement
- keep order traversal simple and predictable
- prefer intrusive structures when nodes are heap allocated anyway

## Candidate Storage Model

### 1. Intrusive linked child list

Use the existing node linkage model for ordered traversal.

Advantages:

- predictable memory cost
- no reallocation churn
- good fit for arena-owned nodes

### 2. Optional fixed tag slots

For important children, a container may keep direct pointers in a small fixed array or equivalent tagged slot table.

Candidate idea:

- `NodeTag` is a small integer type
- tag lookup is container-local, not globally unique
- common children can be accessed in O(1)

This gives a useful middle ground:

- linked list for render/order traversal
- tag slot for direct replacement or lookup

### 3. Fixed slot table for high-value children

Some containers may benefit from a very small direct child table even without a general tagged lookup API.

Examples:

- `left / center / right`
- `header / body / footer`
- `details / editor / actions`

This is useful when the child roles are known up front and the implementation wants predictable O(1) access without introducing a more general matching layer.

The likely rule is:

- fixed slot table for structurally known children
- optional tag lookup for children that may appear/disappear by role
- linked list remains the canonical traversal order

Baseline sizing guidance:

- default fixed slot budget should stay small
- likely baseline candidates are 4 or 8 slots
- larger containers may opt into 16+ slots or a richer backend when justified

Current leaning:

- baseline default should be closer to 8 than to 32
- 16 or 32 should be considered opt-in territory, not the universal starting point

## `NodeTag` Direction

The lightweight direction is a small integer tag, not a string key.

Candidate shape:

```cpp
typedef unsigned short NodeTag; // UInt16-class storage
enum
{
  NODE_TAG_NONE = 0
};
```

Important constraints:

- tag identity is container-local, not global
- tags are optional
- tags are intended for matching and direct access, not for user-facing lookup
- tags must stay cheap enough for Classic/68k hot paths

Current leaning:

- `UInt16`-class storage is enough for the first design pass
- `0` is reserved for "no tag"
- role-style child identity is the intended use, not large generated identity spaces
- uniqueness is required per `ContainerNode`, not globally across the whole scene tree

This keeps the model closer to "explicit child roles" than to a general virtual-DOM key system.

Recommended practice:

- prefer local named constants or enums over scattered numeric literals
- treat tags as container-role identifiers such as `DETAILS_TAG`, `EDITOR_TAG`, or `POPUP_TAG`
- duplicate tags across unrelated containers are acceptable

## Dirty Reporting Back To `Boundary`

`ContainerNode` should not synchronize directly with the platform.
It should report structural consequences back to the owning `Boundary`.

Preferred direction:

- local child replacement inside a container produces local structure dirty
- the owning `Boundary` upgrades that to `NODE_DIRTY_CHILD`
- `SceneCompositionDiff.fullRebuild` remains a boundary-level decision

This preserves the existing outer contract:

- `Boundary` remains the only scene/platform synchronization owner
- `ContainerNode` remains a local structure tool

The open question is granularity:

- v1-compatible path: any local container child replacement still reports `NODE_DIRTY_CHILD` to the boundary, but the container preserves unaffected siblings itself
- future path: boundary output may eventually distinguish "local child swap" from "boundary-wide child rebuild" if platform contracts need that distinction

The first step should use the v1-compatible path.

### Important limitation of the v1-compatible path

The v1-compatible path is structurally useful, but it does **not** automatically preserve native control identity.

With the current platform contract:

- `NODE_DIRTY_CHILD`
- `fullRebuild = true`
- platform-side context clearing / recreate path

the result is still effectively a native rebuild path.

That means the first container-level child retention step may preserve scene-tree structure information better than today, but it does **not** by itself guarantee preservation of native control state such as:

- focus
- text selection
- transient input state
- popup/menu native selection state

So the first value of `ContainerNode` is likely to be:

- better structural locality
- better future diff hooks
- clearer retention metadata

not immediate end-to-end native control retention in all `NODE_DIRTY_CHILD` cases.

To preserve native control identity, one of the following will eventually be needed:

1. a more local platform sync contract than today's boundary-wide `fullRebuild = true`
2. a transitional native-context retention path for selected control types

This also means the first version does **not** need per-container trackers.

Preferred first-step rule:

- state is still tracked at the owning `Boundary`
- `Boundary` marks or drives the affected `ContainerNode`
- `ContainerNode` performs local child replacement
- `Boundary` remains responsible for the outer dirty report

This keeps the first implementation cheap and avoids turning `ContainerNode` into a second state-tracking layer.

## Identity Strategy

The current problem is not only replacement, but selective retention.

Desired behavior:

- swap the branch child that changed
- retain siblings such as `EditText` / `PopupMenu`
- keep native context identity for unaffected children

This suggests an identity rule closer to:

- boundary identity stays the same
- container-local child identity is resolved by slot or tag
- unchanged slot/tag children are retained in place

This is deliberately narrower than a generic keyed tree diff.

Identity should therefore be explicit at the structure level, not inferred from props.

That is especially important for controls with native state, such as:

- `EditText`
- `PopupMenu`
- future scrollable or focusable controls

Those nodes may carry native state that is not reducible to props equality:

- focus
- selection
- scroll position
- platform-side transient control state

For those cases, slot/tag-based identity is a much better fit than props-based matching.

### DSL Expression Direction

The exact DSL spelling is still open, but the use case should be visible from the call site.

Candidate examples:

```cpp
VStack()
  << Tag(101, EditText(&nameState))
  << Tag(102, PopupMenu(items, count).selectedIndex(&selectedIndex));
```

or, if it fits the existing DSL style better:

```cpp
VStack()
  << EditText(&nameState).tag(101)
  << PopupMenu(items, count).selectedIndex(&selectedIndex).tag(102);
```

Current leaning:

- public DSL entry should likely live on `NodeDefinition`
- runtime storage should live on `Node`
- structural layers (`ContainerNode`, `Boundary`, targeted testing access) should be the primary readers

However, there is an important design question:

- should `tag` be a general node attribute
- or should it be a container-level structural role marker

Current leaning:

- identity tags should be treated primarily as structural/container metadata
- not as a general visual attr

### Retention Priority

The intended matching priority is:

1. same tag => same child role candidate
2. same node kind => candidate for node/context retention
3. compatible props change => prefer update/relayout over replacement

This means:

- props equality alone should not define identity
- tag is the primary structural identity signal
- props and node kind decide whether retention is safe

Examples:

- same tag + same `EditText` kind + changed text/attr => prefer retain/update
- same tag + same `PopupMenu` kind + changed selected/value props => prefer retain/update
- same tag + `Button` -> `Text` kind change => replace

In short:

- same tag => try retain
- same tag + compatible kind/props => keep node/context when the surrounding contract allows it
- same tag + incompatible kind => rebuild the tagged child

## `NodeComposition` Relationship

The current direction should make better use of `NodeComposition` instead of bypassing it.

Important distinction:

- `NodeComposition` holds temporary compose-time structure information
- `ContainerNode` holds the runtime structure metadata and child-access cache needed to make those decisions practical

That suggests the following split:

1. previous `NodeComposition` vs current `NodeComposition`
   - compare temporary child structure
   - determine retain / replace / retire candidates
2. `ContainerNode`
   - provide structural metadata and child lookup/cache
   - expose the local identity information needed by the diff/apply path
3. `Boundary`
   - apply retain / replace / retire decisions
   - own lifecycle/state/dirty reporting

This is important because it means `ContainerNode` does not need to invent a completely separate diff language.
It can consume structure-level comparison results derived from `NodeComposition`.

Current leaning:

- `NodeComposition` should remain the comparison source of truth
- `ContainerNode` should remain a structural metadata/cache layer
- `Boundary` should remain the runtime diff/apply owner

This also lowers the cost of change now:

- existing `NodeComposition` is underused today
- early implementation can afford a structural redesign here if it yields a cleaner long-term model

## Create / Build Ownership Path

This is the largest remaining design question.

`ContainerNode` cannot safely own an unconstrained "just give me a `Node *`" replacement API.

That would immediately raise unresolved questions:

- who allocated the node
- whether it belongs to the boundary arena
- whether it came from ordinary `new`
- who is responsible for retire/sweep
- who is allowed to destroy it

### Current leaning

Allocation ownership should remain with the owning `Boundary` / composition build path.

That suggests this split:

- `NodeComposition` declares what structure is desired
- diff logic compares previous/current composition data
- child-holding node metadata provides local tag/slot/cache information
- `Boundary`-owned build context creates any newly required nodes

In other words:

- child-holding nodes provide structural metadata and child-access cache
- `Boundary` remains the allocation/lifecycle owner

### Why raw `Node *` is the wrong first API

A runtime API shaped like this is too weak:

```cpp
replaceTaggedChild(tag, nodePtr);
```

because it hides the most important ownership question.

The safer direction is something like:

- `NodeDefinitionBase &`
- `NodeBuildContext`
- or another explicit build/factory object tied to the owning boundary

So the container layer requests structure, but the owning build path decides how node instances are materialized.

### Candidate direction

Conceptually, the runtime child API wants three responsibilities:

1. find existing retained child by slot/tag
2. request creation/attachment of a new child through a boundary-owned build path
3. retire/detach an old child without immediate unsafe destruction

This points toward responsibilities of this shape:

```cpp
findTaggedChild(tag)
buildRetainedChild(tag, def, buildCtx)
retireTaggedChild(tag)
```

The exact names may differ, and they may belong to an internal boundary-owned apply object rather than to `ContainerNode` itself.
The ownership split matters more than the exact spelling.

### Why this matters for the arena

If child creation continues to flow through the owning boundary build path, then:

- arena allocation policy stays centralized
- selective retention can be layered on top
- retire/sweep can remain boundary-owned

That is a much safer starting point than allowing ad-hoc external node construction.

## `NodeCompositionDiff` Shape

The diff result should stay temporary and local.

Current leaning:

- persistent data should store structural metadata and retained-child cache
- diff results themselves should be temporary compare/apply data
- avoid designs where every child-holding node permanently stores a large diff object

### First-step shape

The first useful shape is intentionally small:

- boundary-local or container-local diff scope
- flat entry list
- action per entry
- empty diff represented by `entryCount == 0` or equivalent

Candidate entry information:

- slot or tag
- action (`retain`, `replace`, `retire`)
- previous child reference/index
- current child definition/reference

### Storage direction

The preferred direction is:

- temporary scratch storage owned by the boundary apply path
- reusable work buffer if needed
- no mandatory tree-wide permanent diff allocation

This means:

- metadata is persistent
- diff result is temporary

### Why not nested vectors

A large tree-shaped nested vector diff is a poor fit for current Loka goals.

Reasons:

- too much allocation/reallocation pressure
- awkward for 68k-era targets
- overkill when the intended comparison scope is boundary-local and child-container-local

If a tree-shaped representation is ever needed, the more natural direction is:

- flat per-container diff for the common path
- optional decorated diff tree using intrusive links, not deep vector nesting

### Buffer sizing and algorithm hints

Algorithm hints can guide temporary buffer sizing and work planning.

Examples:

- `CONTAINER_ALGO_REBUILD`: no local diff buffer needed
- `CONTAINER_ALGO_STABLE_TAGGED`: work is bounded by tagged child count
- `CONTAINER_ALGO_APPEND_PREPEND`: work is bounded by visible edge changes

## `NodeCompositionTransaction` Direction

One useful way to keep this design manageable is to separate:

- `NodeComposition` as a snapshot
- `NodeCompositionDiff` as a temporary comparison result
- `NodeCompositionTransaction` as the working execution scope

That transaction-like scope is attractive because it gives one place to hold:

- previous composition snapshot
- current composition snapshot
- temporary diff result
- scratch/work buffers
- retire list / sweep candidates
- fallback state such as `fullRebuild`

### Why this helps

The main benefit is memory/lifecycle clarity.

Without a transaction scope, too many temporary and semi-retained pieces risk becoming scattered:

- composition snapshots
- compare results
- retained-child candidates
- retired-child lists
- fallback state

With a transaction scope, those can be grouped into one explicit boundary-local unit.

### Current leaning

The most natural ownership looks like:

- `Boundary` owns and drives the transaction
- `NodeComposition` remains a snapshot/input concept
- `NodeCompositionDiff` remains a temporary result inside the transaction

This would make it easier to reason about:

- when temporary diff memory can be discarded
- when retired children move into sweep
- when fallback to `fullRebuild` is chosen
- where compare/apply/sweep boundaries begin and end

### First-pass expectation

The first implementation does not need a heavyweight general transaction framework.

It only needs an explicit enough boundary-local working scope so that:

- compare
- apply
- retire
- sweep/fallback

do not leak lifetime responsibilities across unrelated objects.

## `composeNode` To Slot/Tag Mapping

One missing piece is how ordinary composition code declares structural identity.

Today, dynamic composition is typically written in a flat style:

```cpp
void composeNode(NodeComposition &c)
{
  if (condition)
  {
    c.declare(A);
  }
  else
  {
    c.declare(B);
  }
  c.declare(C);
}
```

If `ContainerNode` introduces slot/tag-based retention, the composition side must be able to say which child role is being declared.

### Candidate API direction

The exact spelling is open, but the first implementation should likely expose one of these structure-level entry points:

```cpp
c.declareTagged(101, A);
c.declareTagged(102, C);
```

or

```cpp
c.declareSlot(DETAILS_SLOT, A);
c.declareSlot(FOOTER_SLOT, C);
```

This is preferable to leaving slot/tag identity implicit in raw declaration order.

### Example: branch slot + stable sibling slot

```cpp
void composeNode(NodeComposition &c)
{
  if (condition)
  {
    c.declareTagged(DETAILS_TAG, Text("Details"));
  }
  else
  {
    c.declareTagged(DETAILS_TAG, Button("More"));
  }

  c.declareTagged(EDITOR_TAG, EditText(&textState));
  c.declareTagged(POPUP_TAG, PopupMenu(items, count));
}
```

In that model:

- `DETAILS_TAG` is a branch replacement candidate
- `EDITOR_TAG` and `POPUP_TAG` are stable sibling retention candidates

### Current leaning

- slot/tag identity should be declared from `NodeComposition`
- `ContainerNode` should consume that declared structure
- raw declaration order may still exist, but it should not be the only identity source for retained child replacement

That keeps the concept closer to child retention and away from appearance/state props.

The key problem is not "can a branch be swapped".
That already works.

The real problem is:

- can a branch be swapped
- while unrelated siblings keep identity
- without forcing a boundary-wide rebuild

That is the design center for `ContainerNode`.

## Container Algorithm Direction

The container should not be forced to infer everything.

The preferred baseline direction is:

- use an explicit container algorithm hint
- keep the default algorithms simple
- allow smarter inference or custom strategy only as later opt-in paths

Candidate baseline algorithms:

- `CONTAINER_ALGO_REBUILD`
- `CONTAINER_ALGO_STABLE_TAGGED`
- `CONTAINER_ALGO_APPEND_PREPEND`
- `CONTAINER_ALGO_SORT_ONLY`

Possible future algorithms:

- `CONTAINER_ALGO_INFER`
- custom strategy/functor-backed variants for larger modern targets

Current leaning:

- start with explicit algorithm selection
- keep inference as an optional future path
- treat custom strategy as an advanced extension, not the first public requirement

### Why algorithm hints fit Loka

This approach keeps the system:

- explicit
- predictable
- cheap enough for 68k-era targets

It avoids paying the cost of a full generic reconciler in the common path.

### Safe fallback rule

If an algorithm encounters a case it cannot handle with confidence, it should be allowed to fall back immediately to boundary-level rebuild semantics.

That means:

- retain/reuse is an optimization attempt
- correctness still comes from the existing `NODE_DIRTY_CHILD` / `fullRebuild = true` fallback

This is an intentional design rule, not a failure mode.

In short:

- try the local algorithm first
- if the case violates its assumptions, escape to `fullRebuild`

That keeps the first implementation safe while still leaving room for more capable algorithms later.

### `SORT_ONLY` as a future-friendly example

Even before a full move/reorder system exists, a `SORT_ONLY` algorithm hint is still meaningful.

If a container knows that:

- the child set is expected to remain the same
- only the order may change

then a container-local compare can, in principle, produce a reorder-oriented result instead of a rebuild-oriented result.

The first implementation does not need to solve every detail of reorder application.
It only needs to leave room for this shape:

- same retained children
- changed order
- fallback to `fullRebuild` if the assumption is violated

This makes `SORT_ONLY` a useful bridge concept:

- immediately useful as an explicit intent marker
- later extensible into reorder-aware runtime behavior
- eventually useful for animation-friendly retained movement

## Diff Result As Future Animation Substrate

The immediate purpose of local container diff is structural retention.

However, the same diff result can later become a useful animation substrate.

Examples:

- branch enter/exit
- card shuffle
- local reorder animation
- retained sibling transitions

That does not mean animation must be implemented in the first pass.
It means the diff result should avoid painting itself into a corner.

A good first-step result model can start small:

- retain
- replace
- retire

and later extend toward:

- insert
- remove
- move

without changing the basic container-local comparison model.

## Menu Relationship

The same structure appears in menu rebuilding.

Current pain points:

- enabled/title/check updates often want local item replacement or local apply
- full menu rebuild is coarse and platform-sensitive
- macOS menu tracking makes whole-bar replacement particularly fragile

A lightweight container/tag model could help by making menu subtree replacement more local:

- keep stable siblings
- replace only the item/submenu branch that changed
- reduce pressure on global rebuild/apply timing

This does not solve all menu issues by itself, but it likely becomes a useful building block.

## `for` / List Relationship

Future `for` / `LazyColumn` work will need more than static slot replacement.

Still, a lightweight container model can provide a useful foundation:

- ordered intrusive child list
- optional tags for special children
- specialized list container for repeated items

This means list work should probably be layered on top of `ContainerNode`, not force `ContainerNode` to solve virtualization by itself.

`showIf` and `for` should therefore be treated as related but not identical cases:

- `showIf`: primarily a local branch-slot replacement problem
- `for`: a repeated-child identity problem

Both may share the same parent `ContainerNode`, but they should not be forced into the same first-pass implementation.

The likely implementation order is:

1. `showIf` / local branch replacement
2. append/remove list behavior
3. move/reorder behavior
4. animation-aware retained movement

That staged order keeps the first implementation useful without pretending to solve the hardest case immediately.

## Scaling Beyond The First Step

The first `ContainerNode` should stay deliberately lightweight.

However, the design should leave room for a more capable container implementation later.

Examples of future pressure:

- larger G3+ era applications with many active nodes
- move-heavy UIs
- card shuffle / reorder style animations
- richer retained local diff inside a single boundary

The intended direction is:

- stable outer `Boundary` contract
- stable outer `ContainerNode` role
- replaceable internal child matching/storage strategy

That allows the project to start with a lean local-diff container and later introduce a smarter implementation without rewriting scene semantics.

In that sense, `ContainerNode` should be viewed as a foundation, not as the final sophistication ceiling.

## Candidate API Shape

Exact names are open, but the model should stay explicit.

Possible responsibilities:

- `findTaggedChild(NodeTag)`
- local slot/tag lookup helpers
- retained child metadata access
- `childAtSlot(...)` for fixed-layout containers

The first implementation should remain intentionally small and specialized.

## Base Class vs Helper

### Preferred direction: `ContainerNode` as a `Node` subclass

Reasons:

- it keeps structural ownership visible in the scene tree
- it fits the current `NodeKind` / `asXxx()` access style
- it avoids building a parallel helper hierarchy that would have to mirror lifecycle and child state indirectly
- it keeps later features such as custom hit test, local diff, and child-slot lookup on the same object that already owns the children

This also means future checks are straightforward:

- `node->asContainerNode()`
- `node->nodeKind() == NODE_KIND_CONTAINER`

No RTTI is needed.

### Rejected default direction: container logic as a detached helper only

A pure helper layer makes some things harder:

- child ownership becomes split between node and helper
- lifecycle ordering becomes less obvious
- local diff state is easier to desynchronize from the actual node tree

Helpers may still exist internally, but the main semantic type should remain a node.

## Internal Strategy Interface

Even if `ContainerNode` is a concrete node type, its child access / diff strategy should still be swappable behind a narrow interface.

That is useful for two reasons:

1. low-cost Classic/68k defaults can stay simple
2. larger modern targets can opt into richer local matching without changing the public node model

Candidate direction:

```cpp
struct IContainerChildAccess
{
  virtual ~IContainerChildAccess() {}
  virtual Node *findTaggedChild(NodeTag tag) = 0;
  virtual Node *childAtSlot(int slot) = 0;
};
```

This does not need to be exposed widely as a user-facing DSL API.
It can remain an internal implementation seam.

Possible backends:

- linked-list only
- linked-list + fixed slot array
- linked-list + richer matching for larger modern builds

The key point is that the outer semantic type remains `ContainerNode`, while the internal child-matching/storage strategy can vary.

## Portability Direction

For long-lived codebases and larger post-G3 targets, the design should allow the implementation strategy to change without rewriting scene semantics.

That suggests:

- stable outer node contract
- replaceable internal child access strategy
- no assumption that one storage backend is permanent

In other words:

- public structure model: fixed
- internal child matching/storage: replaceable

This makes it possible to keep the 68k path lean while still allowing more capable modern-target experimentation later.

## Scope Control

This should be introduced as an opt-in mechanism.
Not every container or node should pay the cost.

Good first targets:

1. Dynamic branch replacement where sibling controls must persist
2. Menu subtree updates for enabled/title/check changes
3. Future `LazyColumn` / list experiments

Good first non-target:

- do not retrofit every existing stack/container immediately
- start with one specialized container path or one dynamic-branch path and validate the cost model first

## Rollout Direction

There are two separate questions:

1. what the long-term model should be
2. how aggressively the first implementation should change behavior

Current direction:

- long-term, standard child-holding containers (`VStack`, `HStack`, `Grid`, `ZStack`, and similar custom containers) should align with the `ContainerNode` model
- short-term, the first implementation can still be conservative and mostly add structure/metadata hooks without immediately changing runtime semantics everywhere

That means the first rollout can be intentionally limited to:

- storing the structural metadata needed by future local child diff
- preparing child access hooks
- keeping behavior unchanged where possible

This is a useful early-project trade-off:

- the structural model becomes consistent sooner
- runtime risk stays lower because the first pass is mostly additive

If that additive pass proves riskier than expected, rollout should stop before behavior changes spread further.

## Non-Goals

Not part of the first step:

- generic arbitrary key reconciliation for every subtree
- broad public API expansion without a clear user-facing need
- replacing `Boundary` ownership with `ContainerNode`
- solving virtualization and menu rebuild in one pass

## Relationship To Existing Docs

This draft extends, but does not replace, the current dirty/update notes:

- `docs/dirty_update_plan.md`: dirty routing and update scheduling
- `docs/layout_attr_draft.md`: layout/attr responsibilities
- `docs/flow_test_sketch.md`: future automated verification strategy

## Relationship To Current Runtime Findings

Recent runtime work already clarified two important facts:

1. Dynamic branch swapping itself now works when the root is mounted as a real boundary root and observed parent-owned state is routed correctly across boundaries.
2. Same-boundary sibling persistence is still not guaranteed under `NODE_DIRTY_CHILD`.

That means the next design step is not "make Dynamic work at all".
The next step is "make Dynamic child replacement more local without breaking the current boundary contract".

## Immediate Next Step

Before implementation, capture one concrete design pass around:

- `NodeTag` representation
- fixed slot count / storage strategy
- interaction with arena ownership
- how local child replacement reports dirty back to the owning `Boundary`
- whether `ContainerNode` should be a concrete base class or a narrower helper mixed into a small number of existing nodes

Current leaning after this draft:

- `ContainerNode` should be a concrete `Node` subclass
- child access / matching should remain replaceable behind a narrow internal interface
- first implementation should rely on boundary-owned state tracking, not per-container trackers
- `showIf`-style local branch replacement should be the first practical target
