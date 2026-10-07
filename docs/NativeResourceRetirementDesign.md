# Native Resource Retirement

> **Status:** Normative, #1065 frozen ruling v1 with REFUTE 2 amendments and
> owner decisions A/B/C (2026-10-07).
>
> **Owns:** Shared native-resource obligations, consuming publication, safe
> completion, and quiescent shutdown. Exact signatures remain in headers.
>
> **Delivery:** PR 1 provides common machinery and host pins. PR 2 wires Win32,
> PR 3 wires macOS, and PR 4 removes the unused Toolbox owned-PICT path. Until
> their rail PRs land, existing native producers retain their inline releasers.

## Owner and clock

```text
RunApp construction: PlatformContext -> ConfigT -> App
       destruction: App -> ConfigT -> PlatformContext

PlatformContext (noncopyable)
  owns NativeResourceRetirement (value, noncopyable)
    owns held_ <-> Ticket { owner, prev, next, handle, dispose }
    owns queued_ -> Ticket
    owns inFlight_ -> Ticket

rail producer --borrows context--> Reservation --reserves--> Ticket
ImageRecord --releaseUserData--> Ticket --owner--> retirement service
App::reclaimWindows --config_->getPlatformContext()--> retirement service
```

A ticket belongs to exactly one context. Contexts are independent; there is no
process-global queue. Shared Images can outlive windows and Boundaries but must
not outlive their PlatformContext. Controller retirement queues and Boundary
Held storage have different owners and are not replaced by this service.
Native side effects belong behind the platform clock, after logical release;
Image's common releaser still runs synchronously exactly once on last-copy loss
or allocation refusal. The releaser transfers the obligation before ImageRecord
storage disappears. Image and ImageRecord need no special retirement fields.

Tickets have private fields. Only the rail-internal
`app/internal/NativeResourceReservation.hpp` exposes reservation; app-facing
headers do not include it. Rail translation units bind static disposal
functions. This is type-erased, rail-bound disposal, not compile-time typed
disposal and not an application callback registry. Inspection belongs solely
in the existing testing access layer.

## Ticket state machine

```text
reserve (LokaNew failure -> refuse creation)
  |
  v
reserved in held_ --publish non-null handle--> live in held_
  |                                            |
  | cancel / early return / null handle        | last Image copy or
  v                                            | FromNative refusal
cancelled (LokaDelete)                          v
                                             queued_
                                               | detach finite snapshot
                                               v
                                             inFlight_
                                               | dispose, then unlink/delete
                                               v
                                             disposed
```

`held_` is doubly linked so cancellation and transfer cost O(1). `queued_` and
`inFlight_` are singly linked snapshots. Transfer allocates nothing. A ticket's
handle is null while reserved and becomes non-null at consuming publication;
this permits auditing armed reservations without a separate flag or counter.
The handle is recorded by release as well. There is no separate state enum:
list membership and the Reservation's one nullable ticket pointer carry phase.

Reservation is a noncopyable stack RAII helper. Its constructor reserves a
ticket bound to the disposer. A failed reservation refuses creation **before
any native acquisition**, including native temporaries. Its destructor unlinks
and deletes an unconsumed ticket. Before entering Image::FromNative,
`publishImage` consumes the ticket. Both success and Image record/control-block
allocation refusal consume it: refusal synchronously calls ReleaseThroughTicket
and queues the ticket. Disarming only on success would double-cancel it.
A null handle is not published and leaves the ticket armed for cancellation.
Publication on an invalid reservation refuses without consuming the handle.

Pre-publication native failure is construction rollback: the producer releases
its partial native acquisitions inline, and Reservation cancels its ticket.
Reservation does not own those partial acquisitions. Acquisition ordering is
reserve, acquire, finish native construction, publish. Win32 decode reserves
before COM/WIC; capture before GetWindowDC. macOS decode reserves before NSImage
allocation, and capture before bitmapImageRepForCachingDisplayInRect, not just
before retain. Caller replacement remains temporary-build/commit where required;
publication is a consuming operation, not a preserve-old replacement policy.

## Eligible completion and re-entrancy

The only ordinary drain site is the tail of `App::reclaimWindows`, after
`pendingReclaim_.clear()` and `flushingWindowWork_ = false`, inside its entry
guard. App borrows the service through its configuration's platform context;
missing configuration or context is a no-op. This also covers no-window tails.
No rail calls drain directly.

Drain refuses while an Operation is active, including a joined nested close,
or while `inFlight_` is nonempty. It detaches queued work into `inFlight_`, then
for each current ticket calls dispose, advances to current->next, and deletes
the ticket with LokaDelete. The current ticket stays linked throughout dispose,
including the last ticket. This derives the re-entrancy wall from real phase
rather than a boolean. Work queued by disposal waits for the next snapshot.
After a completed drain, inFlight is empty; queued is empty only when disposal
enqueued nothing new.

The guarantee is the **next actual eligible outer completion**, not every turn
or any fixed elapsed-time limit. Existing rail completion reachability is to be
pinned in the corresponding integration PRs: Win32 idle and non-idle tails,
macOS timer ticks, Toolbox foreground/background/no-window paths. WM_QUIT can
bypass a normal tail; context final drain covers shutdown obligations.

Code running a nested modal outside an Operation must not carry a raw native
handle borrowed from an Image across that modal call. Operation exclusion is
not a universal proof of modal safety. Synchronous modal work inside an
Operation defers retirement; a deferred presenter outside it may admit eligible
nested completions. Save-dialog integration (#1131) must observe the same rule.

Disposers are rail static functions only. They must not write State, create an
Image, reserve a ticket, or destroy the context. A Win32 disposer must not drop
Loka values; it must not create a new queued cascade after the last pre-sleep
tail. A macOS dealloc cascade may drop an existing Image, queued for the next
completion. macOS integration supplies an autorelease pool for tick disposal;
the outer application pool must survive the context final drain.

## Shutdown and invalid lifetime containment

Exit order is App, ConfigT, PlatformContext. Config-owned Images dropped after
App destruction still queue into the surviving context. PlatformContext's value
member performs final drain. Entry must be quiescent: no active Operation, no
armed Reservation, no producer, and no in-flight disposal. Lifecycle-audit builds
assert the representable preconditions (Operation, in-flight list, and no held
null handles); no separate producer activity counter is needed in this
single-threaded regime.

At entry count H held tickets once. Drain at most H + 1 snapshots: one initial
queued snapshot, and each further snapshot requires a distinct held ticket to
be released. Quiescence prohibits new reservations or publication. This bounds
snapshot passes, not native disposer duration. The final audit requires **all
three lists zero**. Native resources/runtime services used by static disposers
must still be available during base-member destruction.

For invalid late Image lifetime only, release builds revoke the owner of every
leftover held ticket by setting owner to zero. Its eventual Image releaser
deletes only the ticket and never calls the native disposer or dead owner.
This is **release-build containment of invalid lifetime**, not a valid shutdown
or a promised short-lived leak. The allocation backend must still be valid for
late ticket deletion. Other shutdown precondition violations are not supported
alternate lifetime regimes. Static native Images with retirement tickets are
forbidden; explicit owners must release them before their context dies. Passive
Toolbox PICT_BYTES and borrowed PICT paths remain outside native retirement.

## Admission, cost, and future kinds

No speculative pending-count cap is introduced. Ticket allocation refusal is
not a native-byte budget. Acceptance is measured per workload by PRs 2 and 3:
record baseline and retirement peak bytes/handles, image sizes, burst length,
modal path/dwell, refusal behavior, and recovery after completion. Include
multiple replacements within one outer turn and a LazyView generation swap,
plus ordinary Scrapbook navigation and SimpleViewer replacement. Win32 records
GDI handles. A provisional extra-one-image criterion is workload-specific,
never a universal bound; excess requires an evidence-based decision before
rail acceptance. Never drain from Flow to recover memory.

Reservation and cancellation cost O(1) per attempted acquisition. Publication
adds the existing Image allocations. Final release transfers O(1), allocation
free, per final copy/refusal. Completion checks O(1) and walks only its own
queued snapshot. Shutdown counts its own H held tickets once and processes at
most H + 1 snapshots; no foreign ledger is scanned.

The next resource kind supplies one rail static disposer, one consuming publish
adapter for its value type, and Reservation before native acquisition. Cancel,
transfer, completion, re-entrancy, and shutdown stay in the common procedure.
Do not add a per-kind queue, drain door, registry, or controller identity.

## Review risk profile

The owner explicitly approved six flags with this document as the strong design
note: a new cleanup path; lifetime spanning Boundary and Platform; ticket
pointers across the ImageRecord boundary; changed shutdown behavior; multiple
new members; and orphan containment as a new fallback. The common-first split
reduces simultaneous rail scope, not the mechanism's intrinsic review risk.
Always-on exclusion/refusal/transfer and revocation walls carry runtime safety;
audit-only checks diagnose unsupported lifetimes. Host pins and mutations cover
the common mechanism; they do not claim native modal safety or measured rail
retention acceptance.
