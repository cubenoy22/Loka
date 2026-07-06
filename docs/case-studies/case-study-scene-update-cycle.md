# Case Study: The Scene Update Cycle Bug Family (July 2026)

This document records how four related failure modes in the compose/apply
update cycle were found, what each one looks like from the outside, why the
fixes were deliberately parked, and which smells generalize. It exists so the
eventual redesign starts from evidence instead of memory.

Artifacts: issues #44, #45, #59, #60; PRs #51, #58, #61; branch
`integration/scene-update-hardening` (parked — holds the reference fixes and
the failure-mode test suite in `tests/ScenePendingScrubTests.cpp` and
`tests/PhaseGuardTests.cpp` on that branch).

## The four failure modes, as input -> expected -> actual

1. **Reentrant flush (UAF class).** Input: a state write lands while a
   compose (including DETACH teardown) or platform apply is in flight, with a
   pending structural rebuild. Expected: the write is deferred. Actual
   (pre-#51): the write synchronously re-enters refresh mid-cycle; the rebuild
   deletes nodes the outer walk is still touching. Proven by reverting the
   guard: the discriminating fixture aborts with a nested `onChange` firing
   inside the write.

2. **Stale pending target (UAF class).** Input: a nested child boundary is
   NextTick-pending, and the same tick a parent rebuild REPLACEs the subtree
   containing it. Expected: the flush completes and the dead boundary's
   transaction entry is neutralized. Actual (pre-#58): the same-flush
   snapshot/apply walk performs a virtual call through the freed `Node*`
   (ASan: heap-use-after-free in `PendingUpdateRootCursor::next()`). A third
   deletion path (the root wrapper's full-rebuild fallback) had the same hole
   and was caught only by adversarial review of the fix.

3. **Apply-window silent drop (lost update).** Input: a boundary update is
   requested during the apply window (e.g. `markViewDirty` from a platform
   `onChange`). Expected: it is deferred and delivered on the next flush.
   Actual (pre-#61): the end-of-apply `clearUpdateTransaction()` deletes the
   entry while the next-tick request stays armed — the next flush runs and
   applies nothing. Concrete shape: setting an EditText fires a synchronous
   native change notification; a counter label written from that handler
   stays stale until some unrelated interaction.

4. **Tracker-commit swallow (lost update, one layer lower).** Input: a
   boundary writes its own state while its `StateTracker` is in
   `TRACKER_COMMIT` — which is the ambient phase whenever a flush chain runs
   inside the tracker's invalidate callback. Expected: same deferral
   contract. Actual: the dirty mark lands after the settle loop, invalidate
   never re-fires, and the write never reaches the scene transaction at all.

## How each was actually found

- Mode 1 was predicted by audit, but the first fixture could not reproduce it;
  the honest note "a discriminating fixture lands with the follow-up work"
  was kept in the PR instead of overclaiming. The discriminating fixture arrived later,
  from the richer REPLACE machinery built for mode 2.
- Mode 2's third deletion path (full-rebuild fallback) came from bot review of
  the fix — an adversarial micro-read of exactly the code that had just been
  "finished".
- Mode 3 was assumed for months to be the whole story of "writes during apply
  get lost".
- Mode 4 was found only because the mode-3 fix was instrumented before
  trusting it: a counter showed the long-standing pin fixture carried **zero**
  entries into the apply window. The pin's comment had mis-attributed the loss
  to the transaction clear; the write was being swallowed a layer lower. Two
  implementation sessions and several reviews had read that comment and
  believed it.

The division of labor mattered. Adversarial diff-reading (bots) caught holes
in freshly written code (mode 2’s third deletion path, and a live-list/cursor interaction in the
mode-3 fix); instrumented cross-layer verification caught the mis-attribution
nothing else had. Neither mode of review found the other's class of bug.

## Why the fixes were parked

Three independent findings pointed at the same missing concept:

- the mode-3 fix itself worked by adding an epoch stamp — an invisible
  current/next boundary encoded in an integer;
- mode 4 is the same concept absent one layer lower (the tracker has no
  "writes arriving during commit belong to the next transaction" intake);
- review of the mode-3 fix showed apply-window writes land in the same live
  list the in-flight apply cursor is walking.

When three arrows point at one absent structure — a cycle owner with an
explicit current/next intake — patching each symptom hardens the wrong shape.
The branch was parked with the fixes as reference implementations. The
failure-mode tests are the durable asset: they become the acceptance criteria
for the redesign, which is where these bugs get re-fixed deliberately.

The cost of parking was measured before accepting it: none of the four modes
is reachable from the current examples (they need apply-window writes or
dynamic REPLACE under a pending nested boundary), so main carries documented,
pinned, currently-unreachable bugs rather than fresh invisible couplings.

## Smells worth keeping

- **Scheduled-but-empty split brain.** A scheduler flag (`requested_`) stays
  true while the payload it schedules was cleared. The next tick runs and does
  nothing. Whenever "when to run" and "what to run" live in different owners,
  ask what happens when one is reset without the other.
- **A clear() with too-broad responsibility.** One unconditional clear served
  end-of-cycle cleanup, unmount, and detach. The end-of-cycle caller must not
  delete next-cycle work, the others must delete everything — one method
  cannot serve both without a boundary it does not have.
- **Feeding a walked container.** Entries enqueued during apply are appended
  to the same live list the apply cursor is iterating. Anything appended
  mid-walk is neither clearly this cycle's work nor clearly the next's.
- **Invisible boundary encodings.** An integer epoch, a phase flag that
  changes what a write means, a tombstoned node pointer — each encodes a
  structural boundary the types do not show. They work, and each one makes
  the next reader's job harder. Two boxes beat one box with a secret membrane.
- **Inferred pin comments.** A regression pin whose explanation was deduced
  rather than instrumented. The pinned behavior was right; the stated
  mechanism was wrong; every later reader inherited the error. Instrument
  before attributing.

## What transfers

- Keep one reproduction fixture per failure mode, and demonstrate red by
  running it against the pre-fix code (revert the fix commit or stash the
  hunks; instrument when the mechanism is in doubt).
- Treat "the minimal correct fix introduced a hidden version of concept X" as
  the strongest possible evidence that concept X should exist structurally.
- When parking fixes in favor of structure, park loudly: issues stay open
  with the decision recorded, tests stay green on the parked branch, and
  the redesign inherits them as its acceptance gate.
