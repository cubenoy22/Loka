# SimpleViewer open-image characterization

Status: Provisional fixture; runtime verification on the System 7 rig is pending.
Owns: Dialog-free image-load measurements and generated scenario inputs.
Does not own: Production image-load policy or the fragmented-heap fix.
Code truth: `src/SimpleViewerScenarioDriver.cpp`, `tools/scenario/gen_pict.py`.
Verification: `tests/scripts/GenPictTest.py`, launch-only runner pins, and the
Retro68 build. Expected audits must come from the rig.

| Cell | Input | Sequence |
| --- | --- | --- |
| `simpleviewer open-12k` | `SV12K.PICT`, approximately 12,288 bytes | Settle, open, settle, capture |
| `simpleviewer open-50k` | `SV50K.PICT`, approximately 51,200 bytes | Settle, open, settle, capture |

The production MainNode and menu run in a 480×280 content window at (16, 41),
keeping its structure rectangle inside the rig's 640×480 screen. As in
SmirkBench, settled turns exclude Scene invalidation, native retirement and
ToolboxWindow invalidation. Opening occurs on settled turn 2; capture occurs
on the next settled turn.

The test-access layer begins MainNode's existing ImageLoadSession and writes
its chooser result inside a StateTrackerGuard, without showing a dialog.
The result contains `File::Application() << File("SV12K.PICT")` (or the 50 KB
name). `ToolboxPlatformContext::openFile` resolves the application folder
through Process Manager and `FSMakeFSSpec`; unlike a dialog-selected display
path, this needs no entry in `FindChosenSpec`. The production chooser adapters,
capacity check, data-fork read, decode and image commit all run unchanged.

The record contains `image.load` (`ok` or a numeric production flow error
code), `image.width`, `image.height`, actual `image.bytes` read via `GetEOF`,
and `heap.free`/`heap.max_block` sampled immediately before submitting the
load. These are Classic `FreeMem()` and the platform's
`queryLargestContiguousAllocation()` (`MaxBlock()`). The error code is matched
from the completed chooser message using ImageLoadSession's production
formatter through a TEST_BUILD-only friend; unknown messages fail the fixture.
A terminal `succeeded` means the measurement completed, including when the
observed image load failed. Inspect `image.load` to characterize the regression.

The stdlib-only generator creates original seeded monochrome artwork: a
horizontal density gradient and solid rectangles. Its PICT has a 512-byte
zero file header, bounded size/frame, version-2 marker, HeaderOp, uncompressed
BitMap BitsRect with srcCopy and matching source/destination rectangles, and
EndPic. Width is 512 pixels, rows are 64 bytes, and height supplies the requested
file size within 2%. ParsePict recognizes the versioned stream at offset 512;
QuickDraw interprets the bitmap opcode. QuickDraw rendering remains a rig check.
The generator refuses paths outside this repository's `build/`, including
symlink escapes. No downloaded artwork or generated PICT is committed.

Generate manually if inspecting inputs:

```sh
python3 tools/scenario/gen_pict.py --bytes 12288 build/mame-scenario/assets/SV12K.PICT
python3 tools/scenario/gen_pict.py --bytes 51200 build/mame-scenario/assets/SV50K.PICT
```

Build and run from the repository root on the delegator's rig:

```sh
cmake --preset retro68-68k-release
cmake --build --preset retro68-68k-release -j 8
tests/toolbox/run-scenario.sh simpleviewer open-12k --structural-audit
tests/toolbox/run-scenario.sh simpleviewer open-50k --structural-audit
```

The runner generates the selected PICT under `build/mame-scenario/assets/`
and stages it next to the scenario APPL on the dev disk. Generator failure
stops before staging. The existing hcopy extraction produces
`build/mame-scenario/simpleviewer/<cell>/LokaTestsToolbox.audit` and the capture
rectangle. Missing tracked expectations remain a refusal; a first measurement
is not a passing comparison. Inspect both actual audits and captures before
copying the measured audits to `tests/scenarios/expected/simpleviewer/` and
committing them, then repeat the structural commands. Heap facts can vary with
rig/partition state; repeat measurements before accepting a baseline.
Do not run `--update-golden` as part of this handoff.
