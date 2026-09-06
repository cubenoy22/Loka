# Null Paint Contract — Stage 1, PR 1

> Status: Draft (explicitly bounded by the #518 PR 1 v2 owner brief)
>
> Owns: Provisional Null presentation limitations and review risks
>
> Does not own: Native presentation completion, precise movement/removal, or legacy paint routing
>
> Code truth: `common/app/scene/projection/ApplyPaintPlan.hpp`,
> `tests/platform/null/NullScenePlatformController.cpp`
>
> Verification: `tests/PaintContractTests.cpp`, `tests/PaintBaselineTests.cpp`,
> `tests/AllocPinTests.cpp`

A context owns completed presentation history and optional completed placement.
The platform constructs and consumes damage during one Boundary visit; the
Boundary independently consumes legacy apply information. No resident identity
is stored in a plan. Null simulates synchronous rendering, captures current
props, and commits exactly that captured value without an intervening callback
or State write. A separate completion traversal recovers unknown history after
widening. Layout visits submit conservatively and defer completion until after
projection. The presenter receives placement eligibility as a transient argument
because damage precision alone cannot distinguish a pending layout from a
settled unknown-history reconstruction.

Null's scope key is local to its controller. The root projection defines the
single coordinate space; nested Boundary ownership never changes that space.
Projected translations or narrower clips cannot establish exact placement in
this stage. Native rails keep all existing scheduling and broad fallback paths.
Their rollout still requires coverage-aware completion, exposure reconstruction,
old/current movement coverage, native-child delivery, and binding reconciliation.
The common scope-only completion virtual is a compatibility default, not a
native pixel-completion protocol. Null uses typed rendered-value overloads.

Validity invariant: an exact answer requires an attached, successfully placed
self-drawer in the query's scope, settled placement eligibility, and known
presentation history. Delivered retained detach and retirement invalidate both
history and placement; projection invalidates history before replacing placement;
a scope change or refused projection prevents using the old seat. Text additionally
refuses changed resolved style and output that escapes its seat. Failed Text
coverage verification does not establish a new fact. Missing contexts and
unsupported drawers widen rather than disappearing from the answer set.

Null's handler registry is a rail-specific installation contract: canvas contexts
must derive from NativeNodeContext, and replacements for Text/RectSurface must
preserve their concrete typed completion contract. The SimpleViewer geometry
fixtures inherit native refusal. Arbitrary base NodeContext installations are
not safe canvas handlers; the common enumeration procedure performs no cast.

Review risk profile: six triggered flags — new presentation-validity lifecycle;
multiple context facts; Boundary/Platform span; layout/detach behavior; new
fallback behavior; and provisional completion/scope API vocabulary. These require
lifecycle, placement/order, allocation, and submission tests plus native-default
size verification. This is owner-authorized provisional Null scope, not approval
to copy its synchronous assumptions onto a native rail. No new ownership edge,
reclamation path, callback ledger, or State routing mechanism is introduced.
