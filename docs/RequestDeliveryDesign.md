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
| request (`moveCaretTo`) | app-owned `NodeState<LineCursor>` | app (`set`), rail (take: snapshot, then `None`) | rail |

`Reported<T>` has no `set` and no conversion to a mutable handle; Props extract
its write seat through `NodePropsBase::reportSeat`. Facts are named as nouns
(`cursor`, `offset`), requests as verbs (`moveCaretTo`, `scrollTo`).

## From AGENTS.md

- **Delivery sites.** A delivery site is the completion of an entry operation:
  a props apply, an attach, an input action's outer completion, or a retry or
  deferred completion that is itself the entry. A shared helper that several
  entries call (`project`, `replaceProjection`, `restoreCommittedProjection`)
  never takes a request; it returns an outcome to its caller.
- **Bound.** Each delivery takes at most two requests: take, apply (clamped if
  needed), report; re-read the slot once; at most one more take. A request
  still pending after that stays in the slot; the `None` write already marked
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
