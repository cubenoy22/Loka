# Update Pipeline Optimization Notes (2026-03)

This note captures the optimization flow that was completed around the
scene/update/apply pipeline during the 2026-03 pass.

The code and tests are the source of truth. This file preserves the reasoning,
scope, and remaining boundaries of the work.

---

## 1. Goal

The main goal was to stop broad/full redraw behavior that remained even after
boundary-local diff/apply support existed in the codebase.

The most visible symptoms were:

- Win32 flicker during ordinary interactions
- Toolbox full-window redraws after local control/text updates
- `NODE_DIRTY_CHILD` and mixed dirty cycles still collapsing to
  `fullRebuild=true` more often than necessary

The work intentionally prioritized:

- correctness first
- small, reviewable steps
- runtime verification on Win32 and Toolbox

---

## 2. Core Architectural Direction

The main structural change was to make the update/apply pipeline explicit at
three levels.

### Scene-level

- `SceneDirector`
- pending update roots
- `PlatformApplyPlan`

These define the cycle-level contract:

- which roots participate in one update cycle
- whether structure/layout/paint work is needed
- whether the platform should treat the cycle as broad rebuild or local apply

### Boundary-level

- `BoundaryUpdateState`
- `BoundaryCompositionState`
- `BoundaryObservedState`
- `BoundaryRuntimeState`
- `BoundaryLocalApplyInfo`

These define the boundary-local contract:

- what dirty facts were seen
- whether local diff succeeded
- whether native contexts can be preserved
- what local layout/paint work exists for this boundary

### Platform-level

- `PlatformController::onChange(...)`
- `PlatformController::onBoundaryApply(...)`

These separate:

- broad scene/application policy
- localized boundary application/redraw

That split turned out to be the key enabler for later platform-specific
optimizations.

---

## 3. Optimization Flow

### Phase A. Make local apply explicit

The first step was not to optimize redraw directly, but to make the contracts
explicit enough to reason about.

Key outcomes:

- localized `PlatformApplyPlan`
- `BoundaryLocalApplyInfo`
- boundary-local structure/layout/paint dispatch
- platform-local `onBoundaryApply(...)`

This removed a large amount of implicit coupling between compose facts and
platform redraw decisions.

### Phase B. Improve scene fullRebuild accuracy

The next step was to stop treating all child-dirty paths as broad rebuilds.

Important changes:

- child-dirty roots can downgrade `fullRebuild`
- `Scene` no longer relies on root-only `fullRebuild` assumptions
- retain-only child-dirty paths no longer force structure work
- mixed dirty paths (`0x6`, `0x7`) can also downgrade when structure is not
  actually required

This work also required fixing root selection and wrapper behavior:

- outer wrapper roots were sometimes selected even when an inner boundary had
  the real local diff result
- anonymous/single-root wrappers needed fallback diff handling
- root wrappers that had not yet produced snapshots needed narrower treatment

### Phase C. Platform-specific redraw wins

After the scene-level downgrade became trustworthy, platform behavior started to
improve materially.

#### Win32

- local paint rect invalidation became effective enough to reduce visible
  flicker during normal interactions
- the remaining broad behavior became much easier to isolate because
  `onBoundaryApply(...)` and `onChange(...)` were now observable separately

#### Toolbox

Several old redraw problems turned out to be backend-specific rather than core
pipeline problems.

Important fixes:

- stop redrawing the title bar every draw pass
- stop forcing child-dirty through immediate full-window invalidation
- use rect fallback instead of full invalidate when a boundary has layout
  bounds but no explicit bounds hint
- improve `HStack` width distribution so content does not keep pushing sibling
  columns outside the window
- suppress the redundant root-wrapper `apply_fallback_boundary` follow-up redraw
  that happened after local `EditText` / `PopupMenu` updates

That last Toolbox fix was especially important because it was a long-standing
behavior, not a newly introduced regression.

---

## 4. Diagnostics That Helped

The optimization work depended heavily on narrow diagnostics and runtime
verification.

Useful tools during the pass:

- `scene-root-diff` and related scene/root logs
- Win32 redraw stats
- Toolbox debug stats dump
- Runtime verification on:
  - Win32
  - Toolbox / Retro68

The most useful rule from this pass was:

- do not guess from `fullRebuild` alone
- verify which boundary actually composed
- verify which boundary actually triggered platform apply
- verify whether the expensive redraw came from:
  - scene `onChange(...)`
  - boundary-local `onBoundaryApply(...)`
  - platform-local fallback invalidation

---

## 5. Results

The main practical outcomes were:

- Win32 interaction flicker was materially reduced
- Toolbox no longer performs the obvious redundant follow-up redraw after local
  text/menu edits
- `NODE_DIRTY_CHILD` paths now downgrade much more often
- mixed dirty cycles (`0x6`, `0x7`) also became more accurate
- root/wrapper boundaries are no longer as likely to force broad redraw by
  accident

This pass did not finish all performance work, but it changed the problem from
"the redraw path is structurally broad" to "remaining hot spots are now mostly
platform-specific or startup-specific".

---

## 6. Remaining Open Work

The following were intentionally left as future work:

- Toolbox initial display still uses a broad initial path
- Classic/68k startup cost is still high enough to need a dedicated hot-path
  pass
- dirty rect size may still be larger than ideal in some Toolbox flows
- generalized scheduler unification / next-event batching is still future work
- `StaticVsDynamic` remains out of default builds until it is restabilized

Those items belong in `docs/TODO.md`. This note exists only to preserve the
optimization flow and the reasoning that got the code to its current shape.
