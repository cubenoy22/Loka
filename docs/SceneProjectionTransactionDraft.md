# Scene Projection Transaction Draft

This draft captures early design notes for queued Scene/Boundary projection,
layout impact analysis, redraw coalescing, and mixed-Boundary UI updates. It is
not a stable API contract.

## Problem

Boundaries can be nested or placed as siblings. Each Boundary may own its own
State, tracker, composition strategy, and local projection result. However, UI
projection is not completely independent:

- a child Boundary may change size and affect sibling layout;
- a parent Boundary may be structurally replaced and make child updates
  irrelevant;
- a paint-only update may be safely local;
- a layout update may require a broader Scene/Window relayout;
- retained or detached subtrees may have stale native contexts if apply order is
  wrong.

Therefore Loka should not treat each Boundary update as an immediate native
operation. Boundary-local updates should be queued and summarized by the Scene or
Window before platform projection.

## Direction

The core model should be a next-tick Scene projection transaction:

```text
event / State update
  -> Boundary marks dirty
  -> Scene queues Boundary update messages
  -> nextTick flush
  -> Scene analyzes affected roots and parent/child relationships
  -> structure/layout/paint impact is summarized
  -> platform apply plan is built
  -> Window/platform applies relayout, redraw, and native-context updates
```

The queue is not an ordered list of native operations to replay. It is input to
an analysis pass that decides the final required work for the tick.

## Transaction Responsibilities

A Scene projection transaction should answer:

- Which Boundaries are affected?
- Which affected Boundary should become the apply root?
- Does a parent structure update dominate child updates?
- Does a child bounds change require ancestor/sibling layout?
- Can paint be applied locally without relayout?
- Does a retained subtree still have valid native contexts?
- Are any queued updates stale because their subtree detached or was replaced?
- What is the final platform apply order?

Existing concepts such as `BoundaryUpdateResult`, update roots,
`SceneUpdateApplySnapshot`, and `PlatformApplyPlan` already point in this
direction. The goal is to make the contract explicit enough to test and extend.

## Tree Order Normalization

Queued updates should not be applied in request order. The transaction should
normalize them by Scene tree shape.

A simple first model is to build a projection tree snapshot by walking
`INestable` children:

```text
ProjectionTreeEntry
  Node*
  BoundaryNode* if this node is a Boundary
  parent Boundary
  nearest layout-affecting group/container
  depth
  preorder index
```

Then different apply phases can use simple sort keys:

- structure/layout apply: depth ascending, preorder ascending;
- detach/teardown cleanup: depth descending, preorder descending where useful;
- sibling layout grouping: nearest layout-affecting group, then tree order;
- paint/local apply: preorder or platform z-order as appropriate.

This keeps mixed Boundary trees tractable. A Boundary remains a Node in the
tree, while owner/tracker/projection information is attached to the entry as
metadata. Parent-first ordering lets parent structure updates swallow child
updates. Child-first ordering can be used for teardown and release.

Runtime ordering should stay compact and deterministic. Debug display can use a
separate Boundary-biased indentation/depth, such as visually stepping Boundary
levels by a larger amount, so logs make mixed Boundary ownership easy to read
without affecting apply order.

## Parent Structure Dominates Child Updates

If a child Boundary queues local paint or layout work and the parent later
queues a structure replacement in the same tick, the child update should usually
be swallowed by the parent structure update.

Example:

```text
ChildBoundary -> paint dirty
ParentBoundary -> structure dirty / replaces subtree
nextTick -> apply parent structure update; ignore child local paint
```

This prevents stale native updates for children that no longer exist in the
active projection.

## Child Layout Affects Ancestors

If a child Boundary changes its layout bounds and the parent structure remains
stable, the transaction should detect layout impact and promote the update to a
broader layout pass when needed.

Example:

```text
Row
  Boundary LeftPanel  -> width changed
  Boundary RightPanel -> sibling position may change

nextTick -> relayout Row/Scene area; redraw affected regions
```

The parent should not inspect the child's internal State or nodes. It only uses
the child's projection/layout result.

## Measurement And Logical State

Layout changes should usually enqueue measurement and projection work, not force
logical recomposition.

The logical tree should be driven by State, Props, Attr, and composition
identity. Measurement exists to produce layout/projection results such as
preferred size, resolved bounds, dirty regions, and native context updates. A
layout result may add more projection work to the same transaction, but it
should not automatically write back into logical State.

Preferred direction:

```text
logical State/Props/Attr
  -> measurement/layout
  -> projection bounds/paint/native updates
```

Avoid making this the default:

```text
projection measurement
  -> logical State update
  -> recomposition/layout
  -> measurement again
```

That feedback path can create loops and hidden performance cliffs. When an
application really needs measured projection data to affect logical State, the
path should be visible and controllable:

- Flow step;
- distinct/quantized output gate;
- throttle/debounce gate;
- explicit interaction group;
- owner-side apply step;
- typed failure/discard handling where appropriate.

Sizing relationships should prefer layout-facing declarations such as Attr,
constraints, or future `calc`-style expressions instead of feeding measurements
back into State. For example, a proportional width should ideally be a layout
constraint, not a measured value that rewrites application State every tick.

This makes performance risk visible from code shape. Normal layout work remains
inside the projection transaction. Explicit measurement feedback stands out in
review and can be tested, throttled, or rejected.

## Local Paint

Paint-only updates should remain local when possible:

- no structure change;
- no layout bounds change;
- native contexts can be preserved;
- paint bounds are known or safely fall back to the Boundary bounds.

This is an optimization, not the ownership model. If the transaction cannot prove
that local paint is safe, it should fall back to a broader redraw.

## Mixed Boundary Strategies

Different Boundaries may eventually use different composition or projection
strategies. They should still communicate with the Scene projection transaction
through compatible dirty/result/lifecycle contracts.

This allows a standard UI Boundary, a future real-time surface Boundary, and a
custom App/Window dialog presenter to coexist without directly mutating each
other's platform state.

## State Feedback Loops Are A Separate Layer

Projection transactions can coalesce and order platform work, but they should
not be the only solution for State feedback loops.

If Boundary A changes State that causes Boundary B to change State that causes A
to change again, the control point belongs in State/Flow/interaction APIs:

- distinct/quantized output gates;
- throttled or debounced updates;
- explicit interaction groups;
- owner-side apply steps;
- Flow accept/discard/error handling.

The projection transaction should report or expose enough diagnostics to help
debug loops, but the loop policy should remain in the State/Flow layer.

## Example Target: LeftRightResize

An early `0.0.2` sample/test should exercise two sibling Boundaries:

```text
Row
  LeftBoundary
    Make Bigger
    Lock
    Priority

  RightBoundary
    Make Bigger
    Lock
    Priority
```

The sample should verify:

- each Boundary owns its own UI state;
- requesting one side to grow relayouts the sibling correctly;
- locked sides discard resize requests;
- simultaneous requests are resolved through an explicit policy;
- no sibling directly mutates the other sibling's internal State;
- projection work is summarized at the Scene/Window transaction level;
- update-loop diagnostics are understandable when a deliberate feedback loop is
  introduced.

## Tests To Add

Focused tests should cover:

- child paint dirty swallowed by parent structure replacement;
- child layout bounds change promotes Scene/ancestor layout;
- sibling position changes after one Boundary resizes;
- retained subtree detach does not receive stale local native apply;
- paint-only Boundary update can remain local when layout is unchanged;
- queued updates are stable regardless of request ordering within one tick.

## Open Questions

- Should the transaction owner be strictly Scene, or should Window own the final
  projection queue?
- How should App/Window-owned dialog presenters participate in the same
  transaction model?
- What diagnostics should be exposed when a Boundary update is swallowed by a
  parent structure update?
- How much of dirty-rect calculation belongs in common Scene analysis versus
  platform-specific projection?
- How should future real-time surfaces participate without forcing every frame
  through ordinary UI relayout?
