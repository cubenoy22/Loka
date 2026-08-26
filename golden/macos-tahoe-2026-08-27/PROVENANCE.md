# macOS goldens — 2026-08-27 generation

The 0.0.4 set. Baked on **tahoe** at `main = 59e32626` — the 0.0.4
release candidate (the tip after #494; #489 added the macOS standalone UB2
release path between the prior a36148fa work and this RC, so the whole rail is
re-baked here rather than reused).

Ten cells, the ones the release matrix names for this rail:

- **L1 startup** — `scrapbook`, `helloworld`, `tutorial`, `minesweeper`, `floppybird`.
- **L2 representative** — `scrapbook flip-forward-back`, `helloworld bmi-roundtrip`,
  `tutorial increment-summary-toggle`, `minesweeper new-game-twice`,
  `floppybird fixed-step-flaps`.

Every cell passed both layers at this RC: the tracked expected audit matched
byte-for-byte, and the settled capture matched this freshly-seeded rig golden
with **0 differing pixels**.

## Identity-linked cells

`tests/scenarios/startup-golden-identities.txt` declares two settle identities,
and both hold here by hash:

- `scrapbook startup` == `scrapbook flip-forward-back` — `f5d56bae…`. Flip forward
  then back settles on page 1, the picture startup already shows.
- `minesweeper startup` == `minesweeper new-game-twice` — `647b5d36…`. A second
  new game reseeds to the same fixed board.

## The profile records real values here (contrast with Win32 #492)

Unlike the Win32 rail, the macOS `.profile` is honest: `scale_percent=200`,
`depth=24`, `appearance=light`, `os_build=25G76`, captured through
`NSView.cacheDisplayInRect.v1`. The capture-environment guard compared every
declared field in `scripts/rig/macos/rigs/tahoe.ini` against the run before each
golden was written. There is no hand-recorded display note on this rail because
the profile already carries the fact.

## What this set is, and is not

It records **what this version drew in this environment**. It is not a claim that
the pixels are correct.

Known drawing defects present in this build (recorded per the golden ruling,
even where not in these ten cells):

- **#481** — `HelloWorld`'s panel title paints an extra glyph over its first
  letter on all three rails; it is in this build and shows on the `helloworld`
  cells here.
- **#45 item 7** — wrap-mode measurement diverges across rails, silently
  truncating a label on Win32 and macOS.

## Not a long-term baseline

Per the golden-lifetime ruling, a long-term golden is valid only where the OS
environment of the snapshot can be reproduced later. **tahoe is bare metal**: it
is the release-evidence rig and the base for an explained version-to-version
comparison, not a baseline a later mismatch is measured against. The Classic
(MAME) set and the Mavericks VM are the macOS-family sets that carry a
reproducible environment.

The previous generation, `macos-tahoe-2026-08-22/`, is the 0.0.3a re-bake (all
16 registered cells). This 0.0.4 set narrows to the ten matrix-named cells; a
cross-version pixel comparison against it is informational only, for the
bare-iron reason above.

## Producing application binaries (SHA-256)

```
74aab9e085f9758d079f0859a22000107d15fa021217388bbf4a4983498a8d87  LokaScrapbookScenarioMacOS
b03aba4a16eba8e63b6411d05b322ac6b8dbe9a70e6763d82ab20f86c3fa0a0c  LokaHelloWorldScenarioMacOS
edc6a5940ae8e042966d14f84c9d0398bc659bd6df50a7e9670f997148b0af15  LokaTutorialScenarioMacOS
4d23df4e54f274a08a481d397b9bc9c7012aaf5a9187d291709920870da3548f  LokaMineSweeperScenarioMacOS
243078866d08c456d590f34e7482b6a00d9759a961eff77398b1c0705cbf5636  LokaFloppyBirdScenarioMacOS
```

## Why this lives here and not in the tracked tree

Rig-local goldens are untracked: they depend on the machine's fonts and chrome.
Reviewers see the captures through this evidence branch instead.
