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
| request (`moveCaretTo`) | app-owned `Request<LineCursor>` (a private `NodeState<LineCursor>`) | app (`set`), rail (last-wins take: snapshot, then `None`) | rail |
| queue endpoint ([#892](https://github.com/cubenoy22/Loka/issues/892)) | app-owned `RequestQueue<T, N>`: slot + reply + fixed ring | slot: app `post`, binding `consume`/`discard`; reply: rail; ring: endpoint `post`/advance/clear | slot: rail; reply: app; ring: endpoint only (rails never see it) |
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

One settle is one entry operation with one `SettleOwner<Fact>` and seats in a
fixed order. The owner supplies `finishSettle` once and, under `TEST_BUILD`,
`fact`; each `SeatOperation<Request, Fact>` supplies the seven take doors below.
`RailOperation<T>` is the combined single-seat class the rails use, inheriting
`SeatOperation<T, T>` and `SettleOwner<T>`.
`RequestSettlement<Fact>` builds `SeatRunner<Request, Fact>` values and their
pointer array on its own stack and walks them; each runner holds its binding
by value and, under `TEST_BUILD`, its trace row; `FollowUps` stays local to the
driver. The existing
single-seat entry and a two-seat overload share that walk; the command seat
remains page 2b work.
Sources: [#898, ruling items 1–3](https://github.com/cubenoy22/Loka/issues/898),
[#900](https://github.com/cubenoy22/Loka/pull/900).
The stack `RailOperation<T>` holds no context reference. The driver retains
`Node*` and a comparison-only context identity, checking
`node->getContext() == identity` at each seat entry, after each publication,
and after `finishSettle` before continuing.
This liveness wall is always on: retirement at seat k stops seat k+1, the
epilogue, and the trace without reopening the context.

The seven seat doors occur in this order; failed stages skip dependent work,
not the completion path of a still-live take:

1. `admit` re-evaluates phase, status, and ownership for each take; eligibility
   is not a cached input. Supply the binding even when admission defers.
   Empty/deferred admission skips the take, but still reaches the epilogue.
   On admission, `consume()` snapshots the request, then clears a last-wins slot
   to `None` or advances a queued slot directly to its next value (or `None`
   when empty); then check liveness ([#893](https://github.com/cubenoy22/Loka/pull/893)).
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

The owner's `finishSettle` runs once after both ordinary take opportunities
for every seat, folding `FollowUps` from `apply` and `finishTake` into the
completion decision.

The `formReply` overloads deduce their argument types: the same-type overload
keeps `Granted`/`Clamped`/`Refused` selection; different request and fact types
yield `Refused(pending, result)` on failure or `Granted(pending)` on success.
This does not introduce heterogeneous reply or trace shapes; those remain
page 2b work.
Sources: [#898, ruling item 5](https://github.com/cubenoy22/Loka/issues/898),
[#900](https://github.com/cubenoy22/Loka/pull/900).

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
A failed arm adds at most one refusal-only take per seat from that seat's last
admission's still current, nonempty binding: `consume()` (last-wins clear or queue advance),
check liveness/current binding, and publish `Refused(requested, EDITOR_UNAVAILABLE)`. It does not resolve, apply,
report, or write the fact. The driver returns the arm result after this tail,
so native admission stays closed until refusal publication finishes; retirement
returns `FOLLOW_UP_NONE`. A binding discarded during consumption gets no reply
([#893](https://github.com/cubenoy22/Loka/pull/893)).

Under `TEST_BUILD`, `testing::SettleTrace` stores a fixed-capacity history of
value rows: stimulus, admissions, take results, seam results, and fact delta.
Rows are per seat: only a seat that took a request or changed a fact contributes
a row; an empty or deferred seat with neither contributes none. The failed-arm
refusal shares that seat's row (at most two ordinary takes plus one refusal-only
take). Seat 0's `before` is the caller's pre-entry snapshot, never a fresh sample
at settle entry; later seats sample their entry fact after the preceding seat's
publications. `after` finalization and row append happen after the epilogue and
the failed-arm tail: seat k's `after` is the fact captured at seat k+1's entry,
and the last seat's `after` is the owner's final fact.
Sources: [#898, ruling item 4](https://github.com/cubenoy22/Loka/issues/898),
[#900](https://github.com/cubenoy22/Loka/pull/900).
Golden records are grouped per scenario step under each rail's PNG approval;
timer/retry counts are not golden expectations.

The bound is per seat: **two ordinary takes (each may refuse) plus at most one
failed-arm refusal-only take**. With one seat this is exactly the #892 bound,
unchanged. For page 2b's future n-seat operation, the aggregate would be at most
2n ordinary takes plus n failed-arm refusal-only takes; this is not a claim
that the command seat exists.
Sources: [#898, ruling item 7](https://github.com/cubenoy22/Loka/issues/898),
[#900](https://github.com/cubenoy22/Loka/pull/900).

The sourceless `RequestBinding<T>` seat-only constructor, including its default
reply argument, instantiates `RequestDeclarationWall<T>` and refuses
non-coalescable or unspecialized types. Queued bindings pass the queue by
reference alongside the request and reply seats borrowed from that same queue.
Sources: [#898, PR w](https://github.com/cubenoy22/Loka/issues/898),
[#899](https://github.com/cubenoy22/Loka/pull/899).

## Endpoint and queue (#892)

The endpoint ruling is [#892](https://github.com/cubenoy22/Loka/issues/892),
implemented by [#893](https://github.com/cubenoy22/Loka/pull/893).
`RequestQueue<T, N>` supplies fixed ring storage to the non-virtual
`RequestQueueBase<T>`, which owns the slot and reply handles. It is an app-owned
endpoint with one consumer; the rail receives a `RequestBinding<T>` and never
sees the ring. `pending()` is ring occupancy, excluding the published slot.

`RequestBinding::consume()` snapshots the slot, commits the ring head/count,
then publishes the next queued value directly, forcing publication even when
it equals the taken value. Only an empty ring publishes `None`; a plain
last-wins binding always clears to `None`. It touches no endpoint storage after
publication and returns the local snapshot. A synchronous subscriber's post
therefore follows older queued work. Both settlement sites use this door:
the ordinary take and the failed-arm refusal-only take. `discard()` empties
the ring first, unconditionally, then clears a nonempty slot. A cancellation
subscriber's repost consequently enters the emptied endpoint. The caller is
`TextEditorNode::discardPendingRequest`, on binding change or detach.
Sources: [#892](https://github.com/cubenoy22/Loka/issues/892),
[#893](https://github.com/cubenoy22/Loka/pull/893).

`post()` returns `POST_ACCEPTED`, `POST_QUEUE_FULL`, or `POST_INVALID` (`None`);
refused posts change neither slot nor ring. `POST_ACCEPTED` promises one take
and one reply per post until the binding changes or the node detaches.
Discarded slot and ring entries receive no reply; cancellation loses reply
correlation and provides no exact dropped count. The bound is **two ordinary
takes (each may refuse) plus at most one failed-arm refusal-only take**.
Remaining work is delivered by the next props apply within the same flush,
using slot publication to mark props dirty, without a queue timer.
Source: [#892](https://github.com/cubenoy22/Loka/issues/892),
[#893](https://github.com/cubenoy22/Loka/pull/893).

`RequestTraits<T>::coalescable` has no permissive primary; `LineCursor` opts in.
`RequestDeclarationWall<T>` refuses command-like or unspecialized types on all
three plain declaration routes: `StateBatchBase::CreateImmediateState`,
`ComposableNode::state`, and `NodeStateBatch::state`. This covers both
`Request<T>` and `RequestWithReply<T>`; queue declarations bypass the wall.
The check is a negative-array `sizeof` in C++98 and also a `static_assert` in
C++11+. Cost lines: each plain binding gains one pointer; the settlement driver
pays one source null check per take. A queued take mutates its endpoint's ring
and forces publication when advancing to a queued value; discard walks only that endpoint's
waiting entries on binding change/detach.
Source: [#892](https://github.com/cubenoy22/Loka/issues/892),
[#893](https://github.com/cubenoy22/Loka/pull/893).

In [PR #894](https://github.com/cubenoy22/Loka/pull/894), the Toolbox
`smirkbench/text-editor-plain` cell audit has `settle.<step>.<row>` rows
**recorded per scenario step** by `tests/scenarios/TextEditorSettleAudit.hpp`.
Rows carry the stimulus, take outcomes, seam results, and before/after facts;
only settles with a take or a fact change are recorded. Every input step in
that cell emits a fact-changed row, including steps with no takes. This is the
settle-trace golden record; desktop scenario registration remains a follow-up,
not a claim of Win32/macOS scenario golden coverage.

## From AGENTS.md

- **Delivery sites.** A delivery site is the completion of an entry operation:
  a props apply, an attach, an input action's outer completion, or a retry or
  deferred completion that is itself the entry. A shared helper that several
  entries call (`project`, `replaceProjection`, `restoreCommittedProjection`)
  never takes a request; it returns an outcome to its caller.
- **Bound.** Per seat, two ordinary takes (each may refuse) plus at most one
  failed-arm refusal-only take, never another application. With one seat this
  is exactly the #892 bound, unchanged; the future n-seat aggregate for page 2b
  is stated separately above. Pending work stays in the slot
  or endpoint ring; the consumption publication (last-wins `None` or direct
  queue advance) marks props dirty, so the next props apply delivers it.
  Sources: [#892](https://github.com/cubenoy22/Loka/issues/892),
  [#898, ruling item 7](https://github.com/cubenoy22/Loka/issues/898),
  [#900](https://github.com/cubenoy22/Loka/pull/900).
- **Refusal.** A request that cannot be applied (stale line identity, document
  unavailable, missing native resource) is taken and dropped; the fact stays
  unchanged. A last-wins slot reads `None` unless a subscriber reposted; a
  queued slot advances to the next value, or `None` when the ring is empty.
  Source: [#892](https://github.com/cubenoy22/Loka/issues/892).
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
