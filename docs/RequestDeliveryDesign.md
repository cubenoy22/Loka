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

A rail context settles once per admitted operation. Only inside `settle` may it
take a request; the common `RequestSettlement<T>` owns the order (two unrolled
takes, admission re-read per take, snapshot and clear, liveness by
`node->getContext() == identity`, resolve, seam validate, apply, seam report,
reply built from the seam result, liveness, `finishTake`) and one epilogue that
folds the rail's `FollowUps` (repaint only when something was written or
repaired). `finishSettle` returns `FOLLOW_UP_ARMED`, `FOLLOW_UP_FAILED`, or
`FOLLOW_UP_NONE`. A failed arm performs one additional refusal-only take from
the last admission's binding: clear the request, check liveness, and publish
`Refused(requested, EDITOR_UNAVAILABLE)` without resolve/apply/report or a fact
write. Admission supplies that binding even when it defers. Retirement stops
the driver; a discarded binding gets no reply. The refusal shares the settle's
single trace row. The driver returns the follow-up result after publication, so
rails can keep native admission closed until the refusal tail finishes; retired
operations return FOLLOW_UP_NONE and do not reopen their context. Shared helpers never settle. The Null rail is the reference
implementation; the native rails move onto it in the #882 PR series.

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
storage-edit completion (`applyHighlights`), `UNAVAILABLE` refusal. The
open rally on entry completion proposes folding these tails into one owned
`completeEntry(outcome)` per rail.
