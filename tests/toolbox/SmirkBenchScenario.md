# SmirkBench control-draw characterization (#518 PR 2a)

Status: Provisional fixture, build-verified only; MAME measurement is pending.
Owns: Toolbox scenario timing and the procedure for measuring #596.
Does not own: Production paint policy or cross-rail startup parity (#312).
Code truth: `src/SmirkBenchScenarioDriver.cpp` and `run-scenario.sh`.

The driver mounts the production MainNode with the production window dimensions,
title and menus. Its AppConfig owns the model until after App teardown. It
replaces wall-clock advancement with one fixed model step per settled idle turn.
Unlike FloppyBird's autonomous model flap, Add face goes through the existing
`ClickButtonByIdAdapter`, which resolves the Button's emitter and emits inside
a StateTrackerGuard. No synthesized mouse events or test-only MainNode state
mutation doors are needed.

| Cell | Captures | Actions |
| --- | --- | --- |
| startup | post-settle at logical turn 2 | None |
| surface-ticks | post-settle 2, final 33 | 30 steps on turns 3..32 |
| add-face | post-settle 2, post-add-face 4, final 10 | Emit once at 3, then five steps at 5..9 |

Logical turns exclude idle callbacks with pending Scene invalidation, native
retirement, or ToolboxWindow invalidation. Toolbox's `hasPendingSync()` alone
only reports retirement. The native Window's paint queue must also be empty.
Capture never forces a draw or resets statistics. `last.control_draws` means the
controller's last change, not a subtraction or a last-idle-frame counter.

Every action/capture has a durable ScenarioStepTerminal. Each capture appends a
SnapRecord through ScenarioAuditFile::recordVerdict with `rail=toolbox`,
`checkpoint`, surface rectangles, observed Faces text, face count, enabled
Button, content bounds, and the four requested counter fields. Repeated records
are serialized by the existing audit sink without a schema change. A single
terminal record ends the file. These three cells and their expectations are
Toolbox-only: host GUI runners have no SmirkBench application mapping. Future
#312 host support must split out a neutral audit rather than comparing native
counters. Existing FloppyBird/common audits remain unchanged.

SceneTestAccess::platformController is the existing test access path. The
controller had no accessor exposing totalControlDrawCount: its textual summary
has last control draws and collector/apply totals, but omits total control draws.
A const TEST_BUILD-only `debugStatsForTesting()` view is therefore added. It is
borrowed only inside capture; no global, stored controller pointer, new production
field, or paint behavior is added.

## Measured baselines

All four SmirkBench cells have committed expectations under
`tests/scenarios/expected/smirkbench/` (`startup`, `surface-ticks`, `add-face`,
`retained-text-rebind`), measured on the maciix rig, and the atomic golden
bundle covers the full 20-cell registry. `run-scenario.sh` compares every run
against those files; there is no missing-audit allowance anywhere. Do not
overwrite a committed expectation from a single run: a paint-behaviour change
that moves a counter must be measured on repeated runs, explained in the PR that
changes it, and land together with the code that changed the number.

Baselines on main before #518 PR 2b: `surface-ticks` adds 30 control draws over
its 30 steps (`final.total.control_draws - post-settle.total.control_draws == 30`,
the #596 symptom); `add-face` adds one on the action and five over the following
ticks. PR 2b is expected to bring the surface-tick deltas to zero and must update
these expectations deliberately, with the measured numbers.

To re-measure after a deliberate change, run the cell in structural mode,
inspect the actual audit, copy it over the expectation, and re-run:

```sh
tests/toolbox/run-scenario.sh smirkbench surface-ticks --structural-audit
cp build/mame-scenario/smirkbench/surface-ticks/LokaTestsToolbox.audit \
   tests/scenarios/expected/smirkbench/surface-ticks.audit
tests/toolbox/run-scenario.sh smirkbench surface-ticks --structural-audit
```

Adding a cell or changing pixels re-bakes the whole registry, because the bundle
is atomic: with a fixed Git HEAD, run every registered cell with
`--update-golden` (scrapbook cells need `build/host/lrpc` in the same tree),
then run every cell in normal mode and compare the untouched cells' PNGs
byte-wise against the preserved previous bundle.

These cells and their expectations are Toolbox-only: native control-draw
counters are not a cross-rail invariant, and a future host port must split out
a neutral audit rather than compare them.

## Window frame on the rig

The scenario window is `frame(1, 41, 636, 400)`, not production's
`(50, 50, 640, 400)`: the MAME screen is 640x480 and the capture record
(`LokaTestsToolbox.capture`, the structure-rectangle bbox the pixel golden is
cropped by) refuses a window whose structure rectangle leaves the screen. The
scene, model and menus are the production twin; only the frame is rig-local.


## Retained text rebind (#604 PR C; shared with #518 PR 2b)

`smirkbench retained-text-rebind` uses the existing `SmirkBench.FaceCount` Text
and `TextDefinition::applyPropsToNode`, the production retained-apply door,
without modifying MainNode or its composition. Its AppConfig owns two test
States until after the App is destroyed.

At turn 2 capture Faces: 1; at 3 apply A (`Rebind A`), at 4 apply B (`Rebind B`).
Both applies require the same context with the newly captured State, then call
`renderDirty` before any layout/draw can rebuild the TextHit. Turn 5 changes A
and requires no TextHit notification; turn 6 changes B and requires exactly one
TextHit notification. Turn 7 captures `Rebind B updated` and settles. The A check
observes absence of hit dispatch; it does not independently count observers on
the old State.

Measured on the maciix rig: all six steps (`post-settle`, `apply-A`,
`apply-B-replay`, `A-does-not-notify-hit`, `B-notifies-hit`, `final`) succeed,
the terminal succeeds, and the final screen shows `Rebind B updated` in the nav
pane with the Add face button and the surface unchanged. Red side: with
`ToolboxTextContext::onPropsApplied` emptied, `apply-A` is never recorded and the
terminal fails. At that measurement, the other kinds and the "omit the TextHit refresh"
mutation were not pinned by a cell. The #615 follow-up below adds four more
build-verified cells; their native mutation runs remain pending.

## Retained Button rebind (#615 follow-up)

`retained-button-rebind` runs on the existing `SmirkBench.AddFace` Button at
the second settled idle turn. `RetainedButtonRebind`, owned by the driver
AppConfig, supplies enabled A/B States until after App destruction. It preserves
the real click emitter and control tag. Every apply uses `ButtonDefinition`'s
production retained-apply door.

Checkpoints: `apply-A`, `literal-B-before-render`, `enabled-B`,
`B-disables-native`, `A-does-not-enable-native`, `B-enables-native`,
`unchanged-B`. The literal transition uses real ButtonProps assignment and
requires the owned `text_` address to stay equal. The controller's TEST_BUILD
query reads the actual Control Manager title and hilite without applying or
drawing. B must already be the native title before any render. Changing enabled
B disables/enables the control; changing A cannot enable it. Reapplying B must
leave both refresh counters unchanged.

All new checkpoints record `refresh.calls` and `refresh.rows`; every Toolbox
scenario driver's capture also includes them. These are cumulative TEST_BUILD
counters: entries to refreshContextProps and rows searched by that door (including
unsuccessful matches), excluding subscription and diagnostic-query scans.

The checkout used for this follow-up contains 20 existing registry cells; four
new cells make 24. The measured baselines above predate these counter fields.
New and updated expected audits remain a delegator measurement task, and the
strict missing-audit check is unchanged. These probes are build-verified only
until the delegator runs corrected and mutated binaries on the rig. See the
HelloWorld and MineSweeper scenario documents for the other three cells.
