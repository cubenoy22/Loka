# Canvas placement and Show true-arm policy

> **Status:** Guide
>
> **Owns:** Scope, review decisions, and provisional rail limits for #631-b
>
> **Does not own:** LazyFlex/list behavior, scrolling, focus, measurement, native visibility
>
> **Code truth:** `common/app/layout/CanvasLayout.hpp`, `common/app/nodes/nestable/Canvas.hpp`, `common/app/nodes/nestable/Show.hpp`
>
> **Verification:** `tests/CanvasLayoutTests.cpp`

Canvas borrows its viewport from an explicit ancestor owner. Its composition
owns ordinary children; neither cells nor layout introduce state owners.
Layout computes a local range and passes parent origin + world - viewport to
existing projection traversal. There is no saved range, child ledger, offset
observer, or translation scope. Native controls outside the range retain their
previous native placement; Canvas alone does not manage their visibility.

The requested examples include a row touching the viewport's far edge (0..8
for height 160 and cell height 20). This implementation follows those pins:
the ending row is floor(end / cell), inclusive. A nonaligned ending includes
the partial row. Empty viewports lay out nothing. Cross-axis exclusion happens
within the candidate rows; the cost is O(first index + candidate children),
not random access and not necessarily O(intersecting children).

The common handler returns an int content bottom. Horizontal wrapping reports
the full cross-axis content height through this existing Y-result interface;
its main-axis width is available through the shared extent query. A Column
therefore advances by the full content height, not the viewport height.
Toolbox adapts its width-return/Y-output convention at dispatch and refuses
unrepresentable full extents before traversal. macOS and Win32 adapt their
existing int layout state to the shared short traversal, using the same math.
These adapters require target verification. No general widening of legacy
parent layout channels is part of this change; a full content extent can
still exceed the range an enclosing legacy parent accepts.

Show stores a completed BranchPolicies value on its definition and supplies
it for the true arm through the seat interface. The existing branch-plan
policy read combines it with deprecated root annotations. The existing park,
retire, and clock paths consume the result. False and nested arms retain
their own policies. No new teardown step or runtime wrapper is involved.

## Shape review and provisional scope

The four review risk flags are changes spanning State/Boundary/Platform,
new borrowed state input across an ownership boundary, layout/dirty/detach
behavior changes, and new refusal/default behavior. The state input uses the
existing dirty-source registration lifetime; branch destruction uses the
existing retirement paths. Runtime pins cover placement, dirty routing,
refusal recovery, and native ledger removal. C++98 compilation pins modifier
availability on Show and its absence on Match, Keyed, and Canvas.

Ranked design candidates and review findings:

1. Reuse the existing branch policy plan instead of adding a PolicyScope-like
   true-root wrapper. The wrapper would repeat ownership, clone, and unwrapping
   doors. One default seat query and the existing plan policy value suffice.
2. Keep geometry in one handler. Toolbox's result-channel difference needs an
   adapter, not a second geometry implementation or translation stack. The
   dispatch adapters are deliberate platform seams.
3. Make range checks unconditional. A debug-only check would silently wrap
   native coordinates in release builds. Refusals cover int arithmetic as
   well as short narrowing; invalid cell extents cannot reach division.
4. Keep status as the outcome of the latest attempt, not a cached viewport or
   persistent visible-child ledger. The node reads/exposes that outcome and
   a new attempt replaces it. No reset is needed on a separate teardown path.
5. Test through Null traversal and existing lifecycle/ledger APIs. No new
   framework counters, cursor seek API, or visibility metadata are introduced
   just for observability.

Remaining target work: Toolbox build and MAME runtime placement, baseline
conventions for controls, and legacy parent extent limits. Native clipping,
removing stale native placements, and virtualized materialization remain
outside this container's placement-only contract.
