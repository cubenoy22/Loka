# SimpleViewer open-image characterization

Status: Provisional fixture; runtime verification on the System 7 rig is pending.
Owns: Dialog-free image-load measurements and rig-local scenario inputs.
Does not own: Production image-load policy or the fragmented-heap fix.
Code truth: `src/SimpleViewerScenarioDriver.cpp`, `run-scenario.sh`.
Verification: `tests/scripts/ScenarioToolsTest.py`, launch-only runner pins, and the
Retro68 build. Expected audits must come from the rig.

| Cell | Input | Sequence |
| --- | --- | --- |
| `simpleviewer open-sun` | `Sun.pict`, 11,826 bytes | Settle, open, settle, capture |
| `simpleviewer open-bulb` | `Bulb.pict`, 13,810 bytes | Settle, open, settle, capture |

The production MainNode and menu run in a 480×280 content window at (16, 41),
keeping its structure rectangle inside the rig's 640×480 screen. As in
SmirkBench, settled turns exclude Scene invalidation, native retirement and
ToolboxWindow invalidation. Opening occurs on settled turn 2; capture waits on
subsequent settled turns for a decided load outcome (up to 60 settled turns).

The test-access layer begins MainNode's existing ImageLoadSession and writes
its chooser result inside a StateTrackerGuard, without showing a dialog.
The driver resolves `File::Application() << File("Sun.pict")` (or `Bulb.pict`)
through Process Manager and `FSMakeFSSpec`, registers the FSSpec with
`ToolboxPlatformContext::registerChosenFileSpec`, and passes a `FileChooserResult`
carrying the filename as its display path, just like the dialog. The production
chooser adapters, capacity check, data-fork read, decode and image commit run
unchanged.

The record contains `image.load` (`ok` or a numeric production flow error
code), `image.width`, `image.height`, the actual `image.bytes` read via
`GetEOF`, and `heap.probe_covers_image` (`yes`/`no`, `n/a` for the startup
cell): whether the platform's `queryLargestContiguousAllocation()` answer,
sampled immediately before submitting the load, covers the picture. That
answer is the larger of `MaxBlock()` and the room between the application
zone's top and `GetApplLimit()`; it neither compacts nor purges (a `MaxMem`
probe was tried and rejected because its purge bombed the viewer). Raw
`FreeMem()` / probe numbers are not audit fields: they move with the
application's code size. The error code is matched from the completed
chooser message using ImageLoadSession's production formatter through a
TEST_BUILD-only friend; unknown messages fail the fixture. A terminal
`succeeded` means the measurement completed, including when the observed
image load failed. Inspect `image.load` to characterize the regression.

The pristine boot template (`MAME_HDA` in `.env-mame`) must contain data-fork
pictures `:Desktop Folder:Images:Sun.pict` and `:Desktop Folder:Images:Bulb.pict`.
These are rig-local inputs, never committed; the golden bundle keys on the rig.
The runner reads the selected data fork from the template and places it beside
the app on the dev disk under its original name. Its HFS tools use the cell's
isolated `hfs-home`; a missing picture refuses before dev-disk staging.

```sh
HOME="$HFS_HOME" "$HMOUNT" "$MAME_HDA"
HOME="$HFS_HOME" "$HCOPY" -r ":Desktop Folder:Images:$PICT_NAME" "$STAGED"
HOME="$HFS_HOME" "$HUMOUNT"
```

`find_retro68_tool` resolves all three tools. `STAGED` is
`build/mame-scenario/simpleviewer/<cell>/<picture>`; the template is only read.

Build and run from the repository root on the delegator's rig:

```sh
cmake --preset retro68-68k-release
cmake --build --preset retro68-68k-release -j 8
tests/toolbox/run-scenario.sh simpleviewer open-sun --structural-audit
tests/toolbox/run-scenario.sh simpleviewer open-bulb --structural-audit
```

The existing hcopy extraction produces
`build/mame-scenario/simpleviewer/<cell>/LokaTestsToolbox.audit` and the capture
rectangle. Missing tracked expectations remain a refusal; a first measurement
is not a passing comparison. Inspect both actual audits and captures before
copying the measured audits to `tests/scenarios/expected/simpleviewer/` and
committing them, then repeat the structural commands. Heap facts can vary with
rig/partition state; repeat measurements before accepting a baseline.
Do not run `--update-golden` as part of this handoff.

## Rig facts measured on 2026-09-07 (maciix, System 7, 8 MB)

- The Finder needs **three** Tabs to land on the scenario application when
  the PICT is staged beside it (`FINDER_TAB_COUNT=3` in the runner).
- The chosen file goes through the dialog's own door: the driver resolves the
  application-relative FSSpec, registers it with
  `ToolboxPlatformContext::registerChosenFileSpec`, and hands the session a
  `FileChooserResult` whose item carries the display path. A path-less
  application-relative item is treated by the production projection as "no
  file selected" and cancels silently.
- The session advances its Flow over later settled turns; the driver waits
  up to 60 settled turns for a decided outcome (valid image or a completed
  error message) before failing the fixture.
- Generated 1-bit PICT variants bombed after load; generated PixMap variants
  loaded but stalled. The rig's own `Sun.pict` (4-bit PixMap, 11,826 bytes)
  loaded and completed. Whether the generated inputs are invalid or expose a
  viewer defect remains a separate investigation.
- The previous generated-cell audits are stale and removed. Both new cells
  need delegator measurements; no expected audit or successful load outcome
  is inferred from the old cells. This handoff is build-verified only.

## Startup cell

`simpleviewer startup` captures the settled viewer without opening anything.
It is the per-example startup golden the atomic bundle requires before any
other SimpleViewer cell can be staged, and it records `heap.probe_covers_image n/a` with `image.load none`.

Raw heap numbers (`FreeMem`, the capacity probe) are not audit fields: they
move with the scenario application's code size, so an unrelated change would
break the byte-exact expectation. The audit keeps `heap.probe_covers_image`
(`yes`/`no`), which is the fact #614 turns on; measured numbers live in the
PR bodies and the issue.
