# Observable list implementation boundary

> **Status:** Guide
>
> **Owns:** Review rationale and bounded implementation choices for #631 PR a
>
> **Does not own:** UI integration or exact API contracts
>
> **Code truth:** `common/core/ObservableList.hpp`, `common/core/MirroredList.hpp`
>
> **Verification:** `tests/ObservableListTests.cpp`, list pins under `tests/compile`

The model owns its entry buffers and revision. A mirror borrows that model and
owns its working rows and pending operation pages. Neither owner reaches a UI
object. The tracker follows the model-owned precedent in FloppyBird; its lifetime
must contain the model attachment. Completed rows and revisions are read-only at
the public surface.

Batch validation uses identities in the pre-reserved scratch buffer, including
each pending operation's effect. Only after validation succeeds does value replay
start. Failed validation can alter scratch identities, but cannot alter live
entries or consume IDs. This avoids either allocating a validation plan or
repeatedly reconstructing pending identity positions from the operation history.
The cursor must rewind to the same stable sequence for value replay.

The mirror's page log owns its entire chain. Undo removes a tail operation and
reconstructs from the model only if the complete origin still matches. Commit
adapts provisional targets without rewriting the log, so a failed model apply
leaves the original working copy and operation intent available for retry.
Mapping scans the mirror's own pending inserts; it avoids an additional mapping
allocation, with quadratic worst-case replay work in the number of operations.
This is a bounded-memory choice, not a measured speed claim. Large edit histories
should be measured before introducing an additional index owner.

The publishing scope encloses the list's transaction guard, including its
settlement callbacks. An already-open outer transaction remains the outer
owner's responsibility; notification after the door has returned may edit the
model, making a mirror stale in the ordinary way.

## Review risk profile

Four catalog flags apply: new owning/cleanup paths, multiple new member fields,
attach/detach/dirty behavior, and fallback behavior when stale targets vanish.
The frozen #631 scope keeps these together for this data-only change. The
allocation refusal tests, page-release tests, callback probes, stale replay tests,
and non-copyability pins address those four lenses. UI ownership and lifecycle
integration remain explicitly deferred to subsequent #631 work.

Pending operations are valid against the mirror's working origin plus the prior
operations. Model structure changes, content changes, reset, detach, and reattach
invalidate exact replay for undo. Stale commit resolves identities against the
current model, skips vanished real targets, and validates surviving operations
and destinations before changing live rows. A capacity change across attachment
requires constructing a new mirror; cancellation still releases pending pages.

The list algorithms allocate no storage during runtime doors. Existing State
notification and tracker settlement use allocating containers, so a process-wide
allocation-free publication guarantee is outside this change. Tracker state
registration also retains its existing non-refusing allocation behavior; only
the list's buffer reservations have the new typed attach refusal.
