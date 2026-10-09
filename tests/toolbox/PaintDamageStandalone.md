# PaintDamage standalone

Status: 135 PASS / 0 FAIL on the `toolbox-maciix` rig (MAME maciix, 8 MB,
the rig descriptor's boot template), with a byte-identical `LOG.TXT` across
runs and across the Universal Interfaces and Multiversal builds (#1126).
Owns: Toolbox native paint-damage pins that need real QuickDraw and the
Control Manager: exact versus whole-window delivery, presentation history,
control and popup geometry, and busy-cursor borrows.
Does not own: the paint-damage contracts themselves. Their host pins are the
first line (`tests/toolbox/host/*Paint*`, `tests/PaintContractTests.cpp`);
these arms confirm them on the rail.
Code truth: `src/PaintDamageStandaloneMain.cpp`, `run-standalone.sh`.
Expected log: `expected/paintdamage-maciix.audit` (Classic CR line ends, compared
byte for byte).

## When to run it

Run it for any change to Toolbox paint, layout, or control drawing, and update
the expected log in the same change when an arm's outcome or logged value moves
on purpose:

```sh
cmake --build --preset retro68-68k-release --target LokaPaintDamage68K_APPL
LOKA_RUN_WAIT=240 tests/toolbox/run-standalone.sh \
  build/retro68/68k/Release/tests/toolbox/LokaPaintDamage68K.bin \
  --expect tests/toolbox/expected/paintdamage-maciix.audit
```

The app quits itself after its last arm and writes `LOG.TXT` on the dev disk.
A failing arm ends the run, so the arms after it never execute: read the first
`FAIL` line and the diagnostic line before it.

Toolbox CI builds the target (`LokaPaintDamage68K_APPL`), so a source change
that stops it compiling fails there. CI cannot run MAME, so outcomes are only
checked by this run.

## Why the expectations moved (#1126)

The standalone is outside ALL and was not run between #810 (133/0) and
2026-10-09. A fresh build of #810's commit still passes 133/0 on today's rig,
so the drift came from code, not from the rig. Each arm was updated to the
contract of the change that moved it:

| Change | Arms | Now |
|---|---|---|
| `a84cf328` (Toolbox text reuses an unchanged measurement) | `font-busy-layout-24-again`, `-other-family` | the repeat layouts mark their props dirty so they measure again |
| #1015 (Button, EditText, PopupMenu take the offered width) | `popup-exact-setup`, `button-enabled-exact`, `button-label-exact`, `button-label-wider-exact`, `offscreen-button-width-exact`, `column-container-button-no-refusal` | the popup is 164 wide (180 Box less the 16-pixel bar); the title sample sits at the native control's centre; a container-sized Button keeps its rectangle and answers exact |
| #1027 (an unchanged partial repaint keeps its history) | `popup-partial-unchanged-keeps-history`, `button-partial-unchanged-keeps-history` | exact instead of refused |
| #1080 (StateTrackerGuard joins the turn's clock) | `unknown-history`, `popup-detached-refuses`, `offscreen-detached-refuses` | the arm settles the turn before reading a result in the same turn |
| #1162 and #1065 | build | the busy-decode arm sizes its Blob without `LOKA_VERIFY` and decodes through a non-const context |
