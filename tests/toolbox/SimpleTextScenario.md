# SimpleText Classic cells (#1131 PR 6)

Status: Registered; tracked audits are the maciix structural output (2026-10-07).
Owns: The dialog-free Classic scenario protocol and original text fixture.
Code truth: `src/SimpleTextScenarioDriver.cpp`, `run-scenario.sh`.
Verification: Retro68 builds, runner pins, maciix structural runs, MAME
mutations (below), and the complete golden bake.

The scenario config keeps its static window and declares no roster seat, so these cells do not cover New.

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
| save-roundtrip | 2: open ReadMe; 3: production Save over ReadMe, refused; 4: save to absent Saved and inspect; 5: remove rows after row 0; 6: production Save and inspect; 7: open Saved; 8: capture | 3 |

The Tab counts were verified on maciix. Saved is created after launch,
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
Both direct saves invoke the production saveDocument door through the friend
while a current File is present, so no dialog mounts.

## Fixture and facts

`tests/scenarios/fixtures/simpletext/ReadMe` is original repository-authored ASCII:
six short rows, one empty row, LF separators, no terminal separator, tabs or
high bytes. `.gitattributes` disables conversion. The runner copies it raw to
HFS as ReadMe, intentionally without a TEXT type, so the save-refused step
proves on the real rail that Save refuses a non-TEXT file before writing: the
error reads "Not saved: the file is not a text file." and ReadMe's bytes are
unchanged (`refused.*`).

The heap measurement does not use this fixture. It generates `Large` (255 rows
of 31 bytes, 8159 bytes with LF, inside both editor caps), marks it TEXT/ttxt,
opens it through the real open dialog, edits it, and saves it with Command-S.

All cells record row count, each row, current display name, error text, the
Reported cursor's row when present (otherwise `none`), and whether the caret
Request slot still holds a request. `startup` records `caret_row none` with an
empty Request slot: the caret request Main posts while attaching is consumed
without a Reported cursor on Toolbox, while Null reports a row (#1141). Both
saves into Saved read back the
data fork with FSpOpenDF/GetEOF/FSRead/FSClose and inspect FSpGetFInfo. The
`saved.1.*` and `saved.2.*` fields contain bytes, CR/LF counts, decimal type and
creator OSTypes, and exact byte-match results. The independent oracle uses CR
between rows and no trailing separator; the second write must contain only row
0, proving truncation. TEXT is 1413830740 and ttxt is 1953790068. The final reopen
also checks the production reader accepts the CR output and leaves one row.

Native dialogs require an owner manual check; the heap measurement drives the
real open dialog by keys but asserts nothing about it. These cells do not cover Japanese
names or the Retro68 CRT ignoring SetEOF/FSClose results. The raw inspection's
own errors fail the cell; it cannot make the CRT report errors it discards.

## Measurement, mutations and bake

`tests/toolbox/measure-example-heaps.sh simpletext` measured a need of 415.4K
(see `example/SimpleText/Size.r`). The structural audits were produced with:

```sh
for cell in startup open-readme save-roundtrip; do
  tests/toolbox/run-scenario.sh simpletext "$cell" --structural-audit
done
```

Each mutation below was built into the scenario application alone, run, and
restored. Each turned save-roundtrip red for the named reason:

| Mutation | Red because |
| --- | --- |
| Retro68 newline `"\r"` -> `"\n"` | `saved.1` bytes differ (LF, not CR) |
| Prepare creates `'????'` instead of `'TEXT'` | `saved.1.type` is not TEXT |
| Overwrite opens an existing file without truncating | `saved.2.bytes` stays 123, match no |
| Prepare accepts a non-TEXT file | `refused.match` no: ReadMe is overwritten |

The run script cannot extract the saved file, so the cell reads it back in
process. Truncating every write (`"r+b"` for all opens) also stops the audit
file itself from being created; the targeted mutation keeps `"wb"` for absent
files.

The bundle is atomic: adding these cells required re-baking **all 29**
registered cells with `tests/toolbox/run-all-cells.sh --update-golden`, followed
by approval in the rig descriptor and normal verification. A bake cannot
authorize itself. See [SimpleViewer's procedure](SimpleViewerScenario.md) and
[the rig guide](../../docs/LOKA_RIG.md).
