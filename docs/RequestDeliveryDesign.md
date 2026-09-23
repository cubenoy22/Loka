# Request delivery design

How an application asks a rail to change something it does not own (the caret
of a native text control today; selection, focus and scroll position later),
and how the rail answers. The app-facing description is in the
[Programming Guide](ProgrammingGuide.en.md#reported-facts-and-requests); this
page holds the contract the rails implement. Source rulings: #873 (frozen
2026-09-22), the two-take bound accepted on #874, and the entry-completion
ruling from the #879 review (2026-09-23).

## Boxes

| Box | Type | Writer | Reader |
|---|---|---|---|
| fact (`cursor`) | `Reported<LineCursor>` | the node's commit seam only (`TextEditorDocument::moveCaret` / `applyReplace`) | app, through `state()` |
| request (`moveCaretTo`) | app-owned `Request<LineCursor>` (a private `NodeState<LineCursor>`) | app (`set`), rail (take: snapshot, then `None`) | rail |
| reply (optional, `RequestWithReply<LineCursor>::reply()`) | `Reported<Reply<LineCursor>>` inside the request handle | rail, once per take, forced publish | app |

`Reported<T>` has no `set` and no conversion to a mutable handle; Props extract
its write seat through `NodePropsBase::reportSeat`, and the request and reply
seats through `NodePropsBase::requestSeat` / `replySeat`; the Props door has one
overload per handle type and refuses a bare `NodeState`. Facts are named as
nouns (`cursor`, `offset`), requests as verbs (`moveCaretTo`, `scrollTo`).

## Settle (#882)

The EventCall contract is frozen in [#882](https://github.com/cubenoy22/Loka/issues/882).
Its implementation series is [#883 (a, common/Null)](https://github.com/cubenoy22/Loka/pull/883),
[#884 (b, Toolbox)](https://github.com/cubenoy22/Loka/pull/884),
[#885 (c, Win32)](https://github.com/cubenoy22/Loka/pull/885), and
[#886 (d, macOS)](https://github.com/cubenoy22/Loka/pull/886).
The authoritative common sequence is in
[`RequestSettlement.hpp`](../common/app/scene/state/RequestSettlement.hpp);
[`Request.hpp`](../common/app/scene/state/Request.hpp) defines the request/reply boxes.

A rail context settles once per admitted live operation: attach, props apply,
one input action, restore/retry completion, or one deferred callback. Settle is
an operation-completion door, the platform counterpart of a tracker commit.
Nested observations (delegates, selection notifications, `EN_CHANGE`) belong
to their owner's operation; a deferred callback starts a new operation.
Cancelled operations never call a retired context. Shared helpers such as
`project`, `replaceProjection`, and `restoreCommittedProjection` return their
follow-up decision to the owner; they never settle or take requests themselves.

`RequestSettlement<T>` owns two unrolled ordinary takes and one epilogue.
The stack `RailOperation<T>` holds no context reference. The driver retains
`Node*` and a comparison-only context identity, checking
`node->getContext() == identity` before continuing after notifications.
This liveness wall is always on: retirement stops the driver without reopening
the context, running its epilogue, or emitting a trace row.

The eight rail doors occur in this order; failed stages skip dependent work,
not the completion path of a still-live take:

1. `admit` re-evaluates phase, status, and ownership for each take; eligibility
   is not a cached input. Supply the binding even when admission defers.
   Empty/deferred admission skips the take, but still reaches the epilogue.
   On admission, snapshot the request and clear it to `None`, then check liveness.
2. `resolve` checks binding currency. Failure is a refusal, not an early return
   that strands the phase.
3. `validate` asks the seam whether the requested value is valid.
4. `apply` performs the native write, clamp, or repair, returning a
   `RequestApplication` and follow-up decision; check liveness afterward.
5. `report` asks the seam to commit the applied value, then checks liveness.
   Build the reply from that seam result, not native apply success alone.
6. `current` checks the captured binding before publishing the optional reply.
   Publish once per take with forced update, then check liveness. A discarded
   binding receives no reply; an overwritten request was never taken and gets none.
7. `finishTake` receives the reply and original application result, performs
   post-report repair and conditional phase restoration, and returns a follow-up.
   Check liveness; the next admission reads the resulting phase afresh.
8. `finishSettle` runs once after both ordinary take opportunities, folding
   `FollowUps` from `apply` and `finishTake` into the completion decision.

`Granted` means the seam accepted that take, not that subscribers left the fact
unchanged afterward. `Clamped` records requested and applied values; `Refused`
records the reason and leaves the fact unchanged. Replies are not dirty sources.
If native apply succeeded but report refused, `finishTake` restores the native
selection/projection from the committed fact before reopening native admission.
Refusal before a successful native apply does not itself require a native write.

`FollowUps` combines `FOLLOW_NONE`, `RESTORE_QUEUED`, `SCHEDULE_RESTORE`,
`SCHEDULE_HIGHLIGHTS`, `OWNER_FOLLOWS`, `SCROLL_CLEANUP`, and `REPAINT` decisions.
Phase transitions stay synchronous in the rail; timer/selector arming belongs
in the epilogue, while cancellation/coalescing stays in the rail. Win32's
settle-scoped RETRY + UNAVAILABLE conversion belongs in `finishSettle`.
Repaint is requested only after a native write or repair; an empty request
slot alone does not justify it.

`finishSettle` returns `FOLLOW_UP_ARMED`, `FOLLOW_UP_FAILED`, or `FOLLOW_UP_NONE`.
A failed arm adds at most one refusal-only take from the last admission's still
current, nonempty binding: snapshot, clear, check liveness/current binding, and
publish `Refused(requested, EDITOR_UNAVAILABLE)`. It does not resolve, apply,
report, or write the fact. The driver returns the arm result after this tail,
so native admission stays closed until refusal publication finishes; retirement
returns `FOLLOW_UP_NONE`. A binding discarded during clear gets no reply.

Under `TEST_BUILD`, `testing::SettleTrace` stores a fixed-capacity history of
value rows: stimulus, admissions, take results, seam results, and fact delta.
Only a settle that took a request or changed a fact contributes a row; an empty
or deferred settle with neither contributes none. The failed-arm refusal shares
that settle's row (at most two ordinary takes plus one refusal-only take).
Golden records are grouped per scenario step under each rail's PNG approval;
timer/retry counts are not golden expectations.

## From AGENTS.md

- **Delivery sites.** A delivery site is the completion of an entry operation:
  a props apply, an attach, an input action's outer completion, or a retry or
  deferred completion that is itself the entry. A shared helper that several
  entries call (`project`, `replaceProjection`, `restoreCommittedProjection`)
  never takes a request; it returns an outcome to its caller.
- **Bound.** Each delivery performs at most two ordinary takes: take, apply (clamped if
  needed), report; re-read the slot once; at most one more ordinary take. Failed
  follow-up arming adds at most one refusal-only take, never another application.
  A request still pending after those bounded steps stays in the slot; the `None` write already marked
  the node dirty, so the next props apply delivers it.
- **Refusal.** A request that cannot be applied (stale line identity, document
  unavailable, missing native resource) is taken and dropped: the fact stays
  unchanged and the request reads `None`.
- **Binding change.** When the node's document binding changes (a different
  list or request seat in new Props, the end of the node's attachment) the
  pending request is discarded before the new binding is installed; there is
  no replay on attach.

## Where each rail consumes

Null: attach, props sync, input completion. Toolbox: `onPropsApplied`,
`finishInput`, native creation in `render`, `retryProjection`. Win32:
`syncFromNode`, outside-input `handleCommand`, the outer `WindowProc` after a
rejected input is restored, the RETRY timer handler. macOS: ordinary sync
completion, selection completion, `VIEW_CHANGE` completion, the deferred
storage-edit completion (`applyHighlights`), `UNAVAILABLE` refusal. These are
the entry completions governed by the settle contract above; shared projection
helpers are not additional delivery sites.
