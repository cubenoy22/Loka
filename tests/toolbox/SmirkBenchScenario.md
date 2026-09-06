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

## Measure before baking

Expected files intentionally do not exist. This is a byte-exact format with no
comment-placeholder contract. ScenarioToolsTest has an explicit temporary
allowance for only these three missing files; run-scenario still refuses them.
Remove that allowance when all three measured expectations are committed.

On the configured MAME rig, build this branch with the release preset. Preserve
the previous golden bundle outside `build/mame-scenario/golden` before baking
so FloppyBird can be compared against its pre-change captures. For each cell:

```sh
for cell in startup surface-ticks add-face; do
  tests/toolbox/run-scenario.sh smirkbench "$cell" --structural-audit
  # First run must exit 1 ONLY at "missing tracked audit" after extraction.
  # Inspect build/mame-scenario/smirkbench/$cell/LokaTestsToolbox.audit.
  # Require successful terminal, expected checkpoint/action counts, valid
  # capture geometry and a settled launcher log before accepting the sample.
done
```

Do not blindly accept failing output. On unchanged main paint behavior,
`surface-ticks` must have 30 `surface-tick` step records and
`final.total.control_draws - post-settle.total.control_draws == 30`.
The two captures must keep Faces: 1 and button.enabled true and show changed
surface rectangles. Startup has one capture and no action. Add-face has one
emission, five ticks, Faces: 1 -> Faces: 2, and the Button stays enabled. Record
separately the control-draw delta for post-settle -> post-add-face and
post-add-face -> final (the latter is expected to be 5 before PR 2b).
Do not prescribe absolute startup totals; measure them. Repeat runs to establish
byte-identical totals before committing an expected file.

One ordinary Add face does NOT toggle the Button's enabled state (capacity has
not been reached), nor does direct emission synthesize native press feedback.
This cell characterizes that real action; it does not prove #596's stronger
positive control of a genuine Button enabled/text state transition. PR 2b must
supply that additional positive-control evidence, as well as a surface-ticks
delta of zero. Do not mislabel text redraw as Button-state-change evidence.

After inspection, copy the actual files, rerun the structural comparison, and
commit all expectations plus removal of the temporary allowance BEFORE starting
the pixel bake (source identity must stay fixed throughout the bake):

```sh
mkdir -p tests/scenarios/expected/smirkbench
for cell in startup surface-ticks add-face; do
  cp "build/mame-scenario/smirkbench/$cell/LokaTestsToolbox.audit" \
    "tests/scenarios/expected/smirkbench/$cell.audit"
  tests/toolbox/run-scenario.sh smirkbench "$cell" --structural-audit
done
```

The registry now has 19 cells. `--update-golden` does NOT create expected audits
and cannot publish a bundle containing only the three new cells. With a fixed
Git HEAD and worktree status, bake the entire registry:

```sh
while read -r example cell; do
  tests/toolbox/run-scenario.sh "$example" "$cell" --update-golden || exit
done < tests/scenarios/scenarios.txt
```

This includes the explicit new commands:

```sh
tests/toolbox/run-scenario.sh smirkbench startup --update-golden
tests/toolbox/run-scenario.sh smirkbench surface-ticks --update-golden
tests/toolbox/run-scenario.sh smirkbench add-face --update-golden
```

Use the loop OR the explicit commands as part of the full bake, not another
partial bake after publication. Each successful update invokes the following
stage (example shown for surface-ticks; substitute the other cell names):

```sh
python3 scripts/rig/toolbox/classic_golden_identity.py stage-capture \
  --bundle build/mame-scenario/golden \
  --registry tests/scenarios/scenarios.txt \
  --declarations tests/scenarios/startup-golden-identities.txt \
  --descriptor scripts/rig/toolbox/rigs/toolbox-maciix.ini \
  --current-identity build/mame-scenario/smirkbench/surface-ticks/classic-golden-identity.txt \
  --capture build/mame-scenario/smirkbench/surface-ticks/surface-ticks.png \
  --application build/retro68/68k/Release/tests/toolbox/LokaSmirkBenchTestsToolbox68K.bin \
  --source-tree . --example smirkbench --scenario surface-ticks
```

The runner already does this; manual staging is only for an already normalized,
reviewed capture with its matching identity and binary. All 19 captures enter a
sibling `.incomplete` directory; only a complete set is published atomically.
An old incomplete bake with another source identity must be moved aside and
restarted. Do not invent startup-identity declarations for moving SmirkBench
frames. Review any newly printed reference identity separately; baking does not
authorize it or modify the tracked rig descriptor.

Then run all cells normally, including these FloppyBird model commands:

```sh
tests/toolbox/run-scenario.sh floppybird startup --structural-audit
tests/toolbox/run-scenario.sh floppybird fixed-step-flaps --structural-audit
tests/toolbox/run-scenario.sh floppybird startup
tests/toolbox/run-scenario.sh floppybird fixed-step-flaps
for cell in startup surface-ticks add-face; do
  tests/toolbox/run-scenario.sh smirkbench "$cell"
done
```

Compare both FloppyBird normalized PNGs byte-wise with the preserved pre-change
bundle and both actual audits with the unchanged tracked expectations. Keep
screenshots rig-local; do not commit System-rendered pixels.

## Shape review and remaining evidence

Ranked candidates considered for the design and reconciled with the implementation:

1. Rail-local records versus shared audit fields: native counters cannot be a
   cross-rail invariant. Keep the entire new fixture Toolbox-only, explicitly
   labeled, with no shared schema changes. A future host port must split the
   evidence rather than copy these expected files. The new const accessor is a
   test-observation door; without TEST_BUILD it is absent at compile time,
   never a removed safety check.
2. Driver loop versus another Flow owner: keep one logical turn counter and
   reuse TerminalEmitter's completion phase; do not add recorded/remaining-tick
   flags or another generic scheduler. FloppyBird's bootstrap/linger pattern is
   intentionally repeated; application model advancement differs. Future broad
   reuse should extract the common driver shell, not add per-cell flags.
3. Production presentation twin: SmirkBench lacks FloppyBird's protected model
   and productionWindowProps doors. Keep the small window/menu twin explicitly
   cross-referenced, pending a shared presentation extraction with host support.
   Do not add public mutable model access just to simplify this test.
4. Manual registration/build/runner mapping and pending expected-file exception:
   these follow current harness seams, but future cells still copy them. The
   new launch-only runner pins cover all three mappings; missing expected files
   still fail in the real runner. Remove the host-test allowance after MAME bake.

Review risk profile: 4 flags, provisional in this note: (1) new test owner with
AppConfig/model/audit lifetime, (2) test crosses State/Boundary/Platform to drive
and observe, (3) stored idle thunk borrows its explicit AppConfig owner until App
teardown, (4) temporary missing-expectation behavior in the host registry check.
These route ownership review, native cadence measurements, callback teardown
review, and fail-closed runner checks respectively. No production lifecycle,
cleanup, dirty routing, or paint policy changes; no dangerously* call sites.

Primitive members added to existing types: none. New driver primitives:
`borrowedApp_` is set only by setApp and read only by tick; `tick_` is incremented
only by tick and read inside the owner for schedule/audit facts; `lingerRemaining_`
is decremented only by tick and read there for quit timing. Initialization is in
the constructor. The latter two cannot be derived from the completion latch or
each other; no foreign reader exists. Completed capture data is stack-local.

Validity invariant: each capture follows presentation of the last emitted
model/action write. Model advance, Add face, layout feedback, native exposure or
queued invalidation can invalidate that condition. Scene/Window pending checks
and idle separation carry the invariant; actual cadence, late OS exposure, native
draw counts, Finder tab selection and pixel stability still require the MAME
runs above. Mutating the runner's SmirkBench mapping is host-discriminable;
mutating counter sources or settle gates is not. #518 PR 2a's delegator must pin
those on MAME before merge, and PR 2b must change the measured delta deliberately.

Lifecycle review: no new release/drain path; config outlives App and model
outlives Scene; controller/Window views stay within the idle callback; the idle
thunk is owned by the App's Window props with its config alive; no teardown or
reclamation vocabulary changes. Existing creation/clone failure handling remains
with Window/Scene. No refcounts or native handles are added to logical nodes.
