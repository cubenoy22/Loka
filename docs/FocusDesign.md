# Focus design

> **Status:** Normative
>
> **Owns:** Focus fact ownership, participant lifetimes, publication and completion
>
> **Does not own:** Exact API signatures, native focus order, or focus requests
>
> **Code truth:** [SceneFocus.hpp](../common/app/scene/SceneFocus.hpp),
> [FocusPublisher.cpp](../common/app/FocusPublisher.cpp),
> [Window.cpp](../common/app/core/Window.cpp), [App.cpp](../common/app/core/App.cpp)
>
> **Verification:** [SceneFocusTests.cpp](../tests/SceneFocusTests.cpp),
> [FocusPublisherTests.cpp](../tests/FocusPublisherTests.cpp),
> [AllocPinTests.cpp](../tests/AllocPinTests.cpp),
> [ToolboxFocusHostTests.cpp](../tests/toolbox/host/ToolboxFocusHostTests.cpp),
> [Win32FocusTests.cpp](../tests/Win32FocusTests.cpp),
> [MacFocusTests.mm](../tests/MacFocusTests.mm), and the focus compile pins in
> [tests/compile/pins](../tests/compile/pins/)

## From AGENTS.md

Keyboard focus is reported into app-owned `Reported<Focused<K> >` facts, one
per screen by convention; each Scene owns its single publication. Rails call
`App::reconcileFocus` at their outer completion and read native focus at an
admitted completion; native notifications never write the fact, and the
Scene's publication is not duplicated in a rail-owned record.

## The fact

The app declares one `Reported<Focused<K> >` for its screen. A screen that
mixes key types (an enum form beside an id-keyed list) needs one fact per key
type; the Scene still publishes one input at a time, and a move between facts
passes through none (see [Write rules](#write-rules)). Initialize the fact to
`none()`: nothing writes it until an input of that fact is published, so an
initial key would stay stale.

`Focused<K>` has an explicit `none()`; `is(key)` tests a held key, and `key()`
requires a held value. Keys are app data: an enum, integer id, or `ItemId`,
independent of a control's address. LazyFlex can recreate a control while its
model identity survives; identifying an input for any later focus request
would also need that stable identity.

[Focused.hpp](../common/app/Focused.hpp) owns the key wall. `FocusKeyTraits<K>`
has no permissive primary: a mapping must be lossless and fit one machine word.
Application enums opt in; supported integer types and `ItemId` have mappings.
Strings and unregistered keys are refused at compile time by the Props door.

## The Props door

`.focusedAs(fact, key)` on [EditText](../common/app/nodes/controls/EditText.hpp)
and [TextEditor](../common/app/nodes/controls/TextEditor.hpp) stores one
non-template [FocusBinding](../common/app/FocusBinding.hpp): a borrowed write
seat extracted through `reportSeat`, a per-key-type function table, and a key
word. The seat preserves the declaring owner's tracker and transaction.
Identity is the pair (fact state, key), participates in Props ordering, and is
not a dirty source. The app's declaring owner gives the fact its lifetime;
the binding does not extend it.

## Ownership: the shape

```text
[Window] --owns--> [SceneManager] --current--> [Scene] --owns--> [SceneFocus]
    |                                                            |
    |                                      +--membership---------+
    |                                      |                     |
    |                                      v                     |
    |                              [rows inside leaf nodes]      |
    |                                      ^                     |
    |                                      +--publication 1:1----+
    |
    +--owns--> [controller: native read + Null/Toolbox source slot]
                                      |
                                      +--source 1:1--> [row]
```

The kernel stores links and the phase; it knows nothing of bindings or facts.
Comparison, fact writes and audits live in app code, in
[detail::FocusPublisher](../common/app/FocusPublisher.hpp), the single app
`detail` class befriended by `SceneFocus`. Reconciliation reaches it only
through Window; there is no public publisher entry. Participant lifecycle
and binding hooks also enter this private policy to complete their writes.
The controller owns the native read and, on Null/Toolbox, a read-source slot.

## Three links and their lifetimes

Membership joins at `BoundaryNode::composeTree` ATTACH after
`attachStateOwner` succeeds, through `Node::asFocusParticipant`. Only leaves
are queried for joining: no Boundary, composable or nestable node is admitted,
including a Boundary used as a Scene root. Joining is idempotent and silently
refuses RETIRED nodes. It also refuses another live membership (a debug
assert; a no-op in release builds). A refused owner-attach subtree never
joins. See [Boundary.hpp](../common/app/scene/boundary/Boundary.hpp),
[Boundary.cpp](../common/app/scene/boundary/Boundary.cpp), and
[Node.hpp](../common/app/scene/Node.hpp).

Membership survives parking. RETIRED, the row destructor, and the unconditional
`disconnectAll` at the end of `Scene::teardownComposition` unlink it. The
rootless early return also disconnects. `SceneFocus` survives
`REQUEST_DETACH` / `REQUEST_REARM`.

Publication and read source are separate noncopyable, two-ended, callback-free
links; a row can carry both. Cutting is idempotent, and either endpoint's
destructor cuts both ends. Leaving ATTACHED cuts the source even for an
unpublished row; a published row also cuts publication before writing none.
An exchange rewires both publication endpoints before its first fact write.
The controller's source endpoint cuts on destruction.

Row destruction, source destruction and teardown disconnect are no-write
safety nets. Disconnect reads neither bindings nor surviving app facts;
reclamation invokes no living detach hook.
Debug builds assert only that no publication survives `SceneFocus` destruction.

## Reconcile: read, do not listen

Native notifications are not fact-write points. Each admitted outer completion
calls `App::reconcileFocus`, which re-enumerates the live App-owned windows and
calls private `Window::reconcileFocus` once per visited window identity,
skipping close-pending windows. Re-enumeration tolerates observers removing
windows. The ordinary walk uses inline visited storage without allocation;
overflow allocates (see [Cost lines](#cost-lines)).

Four gates precede the native read: App admission flush, SceneManager apply,
Scene run, and the current Scene's Publication phase. The current Scene must
also be attached, have a root, and have a live rail/controller. These checks
remain active in release builds.

[readNativeFocus](../common/app/scene/projection/PlatformController.hpp) has
three answers: cannot answer keeps the current publication and fact unchanged;
answered none clears publication; answered context supplies a candidate. A
candidate needs an app participant type, membership in the current Scene,
logical ATTACHED status and a valid binding. A context that fails any check —
including a text input without `.focusedAs`, which is still a member and can
be a read source, or a control that is not a participant — is treated as
answered none. Inactive native windows decline, so their last value is held.

Reporting occurs at the next admitted completion after the gates clear. A
modal entered from a fact observer suspends that window's reporting and,
through [Busy exclusion](#busy-exclusion), its admission and close; native
focus still moves.

| Rail | Typed participant mark | Completion point | Declared behavior change |
|---|---|---|---|
| Toolbox | Existing edit-control rows, or a row-linked fallback source; native focus takes precedence | Tail of foreground `ToolboxApp::present`, after admission and render; background returns early | Fallback identity is the row, not text state plus rect; key delivery uses the current source context. This prevents stale retired targets and migration between fields sharing text. Native focus, successful native promotion, clipped-away fallback hits and blank clicks clear the fallback. |
| Win32 | `Win32FocusParticipant` uses a `SetPropW` property on EditText/TextEditor HWNDs, removed on detach; read requires a descendant of the active root | After final admission in each message-loop iteration, including `IsDialogMessageW`, before waiting or continuing | Active/click-active `WM_ACTIVATE` with the minimized bit clear restores the current Scene's published control through `SetFocus`, before default processing, only while its typed mark still matches. Deactivation does not restore. |
| macOS | Class-checked first-responder hops through Loka's field/view and delegate owner | End of `MacApp::flushInvalidationsTick`, after admission and pending relayouts | No native behavior change; existing capture/restore still moves focus, then the read follows it. Non-key windows decline. |
| Null | Test-selected `FocusParticipant` connected to the controller's source slot | Startup and scenario-pump completion call App; direct Window completion is available through test access | Simulated focus follows the same publication and source-lifetime rules; no native behavior. |

No rail interprets untyped per-window user data as a participant. Table
sources: [ToolboxFocus.cpp](../apple/toolbox/src/ToolboxFocus.cpp),
[ToolboxPresent.cpp](../apple/toolbox/src/ToolboxPresent.cpp),
[ToolboxEditTextBinding.cpp](../apple/toolbox/src/ToolboxEditTextBinding.cpp),
[ToolboxTextEditorBinding.cpp](../apple/toolbox/src/ToolboxTextEditorBinding.cpp)
(`handleEditClick`, where native focus cuts the fallback),
[Win32FocusParticipant.hpp](../win32/src/context/Win32FocusParticipant.hpp),
[Win32EditTextContext.cpp](../win32/src/context/Win32EditTextContext.cpp) and
[Win32TextEditorContext.cpp](../win32/src/context/Win32TextEditorContext.cpp)
(mark attach and detach),
[Win32ScenePlatformController.cpp](../win32/src/Win32ScenePlatformController.cpp),
[Win32App.cpp](../win32/src/Win32App.cpp),
[Win32Window.cpp](../win32/src/Win32Window.cpp),
[MacScenePlatformController.mm](../apple/macos/src/MacScenePlatformController.mm),
[MacEditTextContext.mm](../apple/macos/src/context/MacEditTextContext.mm),
[MacTextEditorContext.mm](../apple/macos/src/context/MacTextEditorContext.mm),
[MacApp.mm](../apple/macos/src/MacApp.mm),
[NullScenePlatformController.hpp](../tests/platform/null/NullScenePlatformController.hpp),
[NullApp.hpp](../tests/platform/null/NullApp.hpp),
[AppTestAccess.hpp](../common/testing/app/AppTestAccess.hpp),
[WindowTestAccess.hpp](../common/testing/app/WindowTestAccess.hpp) (direct
Window completion), and the Null completion pins in `FocusPublisherTests.cpp`.

## Write rules

Write only on difference: even an equal State set would dirty its tracker.
Move the publication endpoint first. Within one Scene, moving between inputs
of the same fact writes the new key once, without intermediate none. Between
different facts, clear the old fact first, then re-check the live publication
edge before dereferencing the target, check ATTACHED, and re-read its binding
after observers before publishing the new key. Membership was checked at
admission; retirement and disconnect cut the edge, so the edge re-check also
protects against their membership loss. An invalid re-read binding writes
nothing.

Observers may briefly see no holder, never two. Reconcile does not loop to
stability: an observer's native focus change is read at the next completion.
Across a Scene swap, a shared fact passes through none when the old published
row leaves ATTACHED during teardown; the new Scene publishes at its first
eligible completion. The stranded-row exception is in [Known
limits](#known-limits).

## Participant lifecycle

Only the published participant writes the fact. Attaching, replacing the
binding of, or detaching an unpublished participant never touches it.
Leaving ATTACHED while published, whether terminal or retained/hidden, cuts
source and publication before clearing the old binding.

Replacing the published binding uses the same exchange: key-only change writes
once; a different fact clears the old fact, then publishes the new key subject
to the observer re-checks above. The applier keeps an old-binding snapshot
until exchange completes. Reclamation writes nothing. The two controls share
[FocusParticipant](../common/app/FocusParticipant.hpp) and publication policy.

## Busy exclusion

`Scene::isBusy` means a run is in progress or that Scene is publishing focus.
App admission, retired-Scene reclaim and close draining skip the busy window's
row entirely. This prevents swap during publication followed by nested reclaim
from freeing the Scene beneath the publication frame, and prevents close
from deleting the window during publication. Pending work remains for a later
flush; no separate busy flag is stored.

## LazyFlex and other seats

An item keeps its fact and key for its structural lifetime. Content updates
apply item Props without re-declaring its children, so they do not reapply a
nested `.focusedAs`; change the key through structural replacement. Ordinary
retirement on scroll-out reports none (except for #912; see [Known
limits](#known-limits)). App data retains the key, not the focus value, and
returning does not restore focus. Parked Match/Show branches retain membership
but lose publication and source.

## App errors, audited not enforced (debug builds)

Two ATTACHED inputs with the same fact and key in one Scene, or one fact
simultaneously published by two live Scenes, are app errors. Debug audits run
only at publication of a new target and replacement of the published binding.
They do not enforce uniqueness in release builds.

Registration can overlap legally: local-rebuild REPLACE attaches the new child
before retiring the old, and a prepared Scene composes before installation.
Therefore registration is not an audit point. A conflict introduced later
remains unreported until the next publication or published-binding replacement;
the cross-Scene audit compares simultaneous publications, not all borrows.

## Known limits

- [#912](https://github.com/cubenoy22/Loka/issues/912): a local-rebuild refusal
  can strand children. A stranded published row can keep its fact HELD until
  teardown disconnects it without writing; disconnect does not repair the fact.
- [#913](https://github.com/cubenoy22/Loka/issues/913): a prepared replacement
  Scene can project early. Publication rejects the wrong Scene's rows by
  construction, but whole-sequence safety is not claimed until that fix lands.
- A refused `attachStateOwner` subtree never joins and cannot publish: this
  failure stays on the safe side of the focus contract.
- macOS capture/restore carry-key cleanup remains deferred under item 8 of
  [#911](https://github.com/cubenoy22/Loka/issues/911). Its existing temporary
  native restoration state is not another Scene publication record.

## Cost lines

The amended [#911 ruling](https://github.com/cubenoy22/Loka/issues/911) supplies
these doors; the App enumeration and checked app-row conversion reflect the
landed implementation in [#915](https://github.com/cubenoy22/Loka/pull/915).
Costs below exclude observer work; debug audit costs are listed separately.

| Door | Caller / frequency | Work and owner of rows visited |
|---|---|---|
| Join | Kernel, each eligible leaf ATTACH | One capability query; O(1) idempotent membership insert; no fact write |
| Leave / retire | Every node lifecycle-fact change, all node kinds | One capability query per node (`Node::asFocusParticipant`); for participant rows, O(1) endpoint cuts, at most one none write if published; O(1) membership unlink on RETIRED |
| Teardown disconnect | End of each composition teardown | O(M) over this Scene's surviving members, normally empty; no writes |
| Reconcile | Each eligible live window, once per completion | Gates, one native read and pointer/type checks; on change O(1) rewire and zero to two direct fact writes; no membership walk outside debug audit |
| App enumeration | Each outer completion | Repeated scans of App-owned group entries; for W windows, O(W squared) group visits and O(W cubed) worst-case visited-identity comparisons, plus close-pending lookups. No allocation within the inline visited capacity of `App::reconcileFocus`; overflow uses O(W) storage and allocates. Non-window group entries also participate in scans. |
| Busy check | App admission/reclaim and close draining, per window | One focus-phase read beside the existing run check |
| Binding replacement | Participant applier | O(1) exchange and zero to two direct writes when published |
| Source replacement | Rail, on native/test source change | O(1) endpoint rewire; no fact writes |
| Debug audit | New publication or published-binding replacement | O(M) current Scene members plus the registry of live `SceneFocus` owners |
| Storage | Scene / participant / controller | One `SceneFocus` per Scene; membership hook plus publication and source endpoints per row; source endpoint on Null/Toolbox controller. No Node field; `Node::asFocusParticipant` is a virtual capability, and `FocusRow` has a checked app-extension type key. |

The allocation pin covers steady-state App enumeration within inline capacity;
it does not certify arbitrary window counts or allocations made by observers.
Exact storage capacity and representations remain in the code.

## Rejected shapes

- Writing the fact from native notifications: no rail delivers a complete set ([#911](https://github.com/cubenoy22/Loka/issues/911)).
- One fact per input (`.focused(aFocused)`): "one of them" is not a type, and it dies with a LazyFlex item ([#911](https://github.com/cubenoy22/Loka/issues/911)).
- `FocusScope` / `FocusField` seat nodes: a Keyed arm misses the enclosing scope, a wrapper is not layout-transparent, and finding the scope is a multi-hop traversal ([#911](https://github.com/cubenoy22/Loka/issues/911)).
- A per-node "Scene resident" bit: it carries no Scene identity, so a prepared replacement's rows could not be rejected ([#911](https://github.com/cubenoy22/Loka/issues/911)).
- A common virtual focus holder implemented once per rail: it repeats the common mechanism across four implementations ([#911](https://github.com/cubenoy22/Loka/issues/911)).

## Not covered here

Asking for focus and focus order are outside this contract. No focus request
door exists today.
