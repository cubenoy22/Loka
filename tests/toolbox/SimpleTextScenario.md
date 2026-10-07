# SimpleText Classic cells (#1131 PR 6)

Status: Staged; tracked audits are predictions pending owner MAME measurement.
Owns: The dialog-free Classic scenario protocol and original text fixture.
Code truth: `src/SimpleTextScenarioDriver.cpp`, `run-scenario.sh`.
Verification: Retro68 builds, runner pins, then the owner's maciix structural
runs and complete golden bake. Predictions are not runtime evidence.

The production Main and menu run in a 480 by 320 content window at (16, 41).
Like SimpleViewer, idle ticks advance only after Scene invalidation, controller
sync and Toolbox window invalidation settle. Actions begin at tick 2, one per
settled tick; the final capture is within the 60-tick bound. Failures publish
available document/file facts with a failed terminal status, then linger and
quit through the same completion path as success. The runner bounds a process
that never settles.

| Cell | Settled ticks | Finder Tabs |
| --- | --- | --- |
| startup | 2: record empty document | 2 (app, cfg) |
| open-readme | 2: open ReadMe; 3: capture | 3 (app, cfg, ReadMe) |
| save-roundtrip | 2: open ReadMe; 3: save to absent Saved and inspect; 4: remove rows after row 0; 5: production Save and inspect; 6: open Saved; 7: capture | 3 |

The owner verifies Tab navigation on the rig. Saved is created after launch,
so it does not participate in launch navigation. `LokaSimpleTextTest68K_APPL.bin`
is 30 characters, below HFS's 31-character limit.

## Chooser delivery

Toolbox presents its StandardGetFile/StandardPutFile synchronously when Show
mounts. The test TU's existing friend name `SimpleTextTestAccess` sets
`operation_` and calls `completeChooser` inside **one StateTrackerGuard**.
Completion returns the operation to NONE before commit; neither derived Show
observes OPEN/SAVE, so no real dialog blocks the cell. This deliberately bypasses
the rail result channel and its Flow hop (covered by PR 5's Null host tests).
It exercises Main completion, commitDocument and the real Toolbox file rails.
No production test API is added.

ReadMe is resolved as an application-relative item through platform openFile,
then captured with `ToolboxCaptureChosenFile`. For Saved, that resolver supplies
the application's vRefNum/parID; an explicit FSMakeFSSpec requires fnfErr before
capture, proving the create path is tested. ToolboxCaptureChosenFile supports
an absent destination: it captures the native address without checking existence.
The direct overwrite invokes the production saveDocument door through the
friend while the current File is Saved.

## Fixture and facts

`tests/scenarios/fixtures/simpletext/ReadMe` is original repository-authored ASCII:
six short rows, one empty row, LF separators, no terminal separator, tabs or
high bytes. `.gitattributes` disables conversion. The runner copies it raw to
HFS as ReadMe, intentionally without type/creator. Measurement stages the same
fixture but types into the fresh editor using the existing key-step grammar;
it does not open a native dialog.

All cells record row count, each row, current display name, error text, and the
Reported cursor's row when present (otherwise `none`). Both saves read back the
data fork with FSpOpenDF/GetEOF/FSRead/FSClose and inspect FSpGetFInfo. The
`saved.1.*` and `saved.2.*` fields contain bytes, CR/LF counts, decimal type and
creator OSTypes, and exact byte-match results. The independent oracle uses CR
between rows and no trailing separator; the second write must contain only row
0, proving truncation. TEXT is 1413830740 and ttxt is 1953790068. The final reopen
also checks the production reader accepts the CR output and leaves one row.

Native dialogs require an owner manual check. These cells do not cover Japanese
names or the Retro68 CRT ignoring SetEOF/FSClose results. The raw inspection's
own errors fail the cell; it cannot make the CRT report errors it discards.

## Owner measurement and bake

Run `tests/toolbox/measure-example-heaps.sh simpletext`, replace the provisional
SIZE values/comment, rebuild production and scenario applications, then run:

```sh
for cell in startup open-readme save-roundtrip; do
  tests/toolbox/run-scenario.sh simpletext "$cell" --structural-audit
done
```

Inspect the actual audits under `build/mame-scenario/simpletext/<cell>/`, replace
the predicted files under `tests/scenarios/expected/simpletext/`, and rerun.
See [SimpleViewer's procedure](SimpleViewerScenario.md) and
[the rig guide](../../docs/LOKA_RIG.md) for structural comparison and golden
approval. The bundle is atomic: adding these cells requires re-baking **all 29**
registered cells with `tests/toolbox/run-all-cells.sh --update-golden`, followed
by approval and normal verification. A bake cannot authorize itself.

Before acceptance, mutate the Retro68 newline to LF, Prepare's FSpCreate type,
and truncation independently: save-roundtrip must fail each time. Restore and
rebuild between mutations. The owner performs all MAME work.
