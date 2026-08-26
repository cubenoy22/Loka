# Win32 (x64) goldens — 2026-08-27 generation

The 0.0.4 set. Baked on the reference Win32 rig **omen** at `main = 59e32626`
(the 0.0.4 release candidate; the prior `win32-x64-2026-08-26/` set was at
`a36148fa`, and the two commits between are a proven no-op for Win32 — see the
version-to-version note).

**Eight of ten matrix cells.** Four examples, each L1 `startup` + L2
representative, all passing both layers (tracked audit byte match + settled
rig-local pixel golden):

- `scrapbook` startup / flip-forward-back
- `helloworld` startup / bmi-roundtrip
- `tutorial` startup / increment-summary-toggle
- `floppybird` startup / fixed-step-flaps

## MineSweeper is blocked, not baked — #496

`minesweeper` startup and new-game-twice are **absent on purpose**. On Win32 the
board settles to an incompletely-painted grid (bottom-right cells unpainted) on
roughly one run in three, and the two-frame settle rule accepts that stalled
frame. Baking would encode a coin-flip. Filed as
[#496](https://github.com/cubenoy22/Loka/issues/496). macOS bakes the same two
cells at 0 differing pixels, so it is Win32-specific. For 0.0.4 these two cells
are recorded as blocked by a known defect.

This is **not** the #459 seam tie: #459 is a ≤2-column ImageView stretch tie the
compare tolerance absorbs (those runs pass); #496 is a 176-column missing-cell
paint that exceeds tolerance and genuinely fails ~1 in 3.

## Identity-linked cells

`scrapbook startup` == `scrapbook flip-forward-back` (flip forward then back
settles on page 1). Both files hash identically, the identity
`tests/scenarios/startup-golden-identities.txt` declares.

## Display environment — recorded by hand, because the profile cannot

The `.profile` files say `scale_percent=100`. That value is wrong on every Win32
capture this rail takes (#492): Loka declares no DPI awareness, so the profile
records a constant, not a fact about the machine. Measured from a
per-monitor-aware probe at bake time:

```
GetDpiForSystem()                   144  => 150%   (primary)
GetDeviceCaps(screenDC, LOGPIXELSX) 144
primary monitor    (0,0)-(3840,2160)   150%   [PRIMARY]
secondary monitor  (486,2160)-(3366,3960)  2880x1800 laptop panel
```

**The desk moved since the 2026-08-26 generation**, which recorded a single
2880x1800 panel at 200% (`GetDpiForSystem()=192`). The app and the capture are
both DPI-unaware, so each run's capture is internally consistent and every cell
verified against its own freshly-seeded RC golden. But a cross-generation pixel
comparison against `win32-x64-2026-08-26/` would mix a code delta (none, see
below) with an environment change (this desk move) — which is exactly why the
bare-iron rails are release evidence, not durable baselines.

## Version-to-version: 2026-08-26 (a36148fa) -> this (59e32626)

`git diff a36148fa..59e32626` reaches the Win32 build only through
`common/app/nodes/nestable/PolicyScope.hpp` — one line adding a **stateless**
interface base to a copy constructor (no data members; a provable no-op). The
other changes are macOS-standalone scripts and a Linux-host test registration.
No `tests/scenarios/expected/` change. So the Win32 output is byte-identical
across the two commits by static proof; the only reason this set is re-baked
rather than reused is the desk move above and the four newly-covered examples.

## What this set is, and is not

It records **what this version drew in this environment**. Not a correctness
claim. Known drawing defects in this build:

- **#481** — HelloWorld's panel title paints an extra glyph over its first
  letter on all three rails; present on the `helloworld` cells here.
- **#45 item 7** — wrap-mode measurement diverges across rails, truncating a
  label on Win32 and macOS.
- **#496** — the MineSweeper incomplete-grid settle described above.

## Not a long-term baseline

Bare metal: the desk moves, Windows updates, the display configuration is not
recorded by anything the rail reads. Release evidence and a base for an
explained version-to-version comparison — not a baseline a mismatch is measured
against. The Classic (MAME) set carries the reproducible environment.

## Producing application binaries (SHA-256)

```
585ddf8d261e7fc8e6fd5ce2b88bf825a2debdc98d5f5b6d8c296d1f8b489d1c  LokaScrapbookScenarioWin32.exe
667bbb816c4d8c8f938f829e34e618d52082fba4788aa8278199de4254425031  LokaHelloWorldScenarioWin32.exe
fbd8db8337f8cd18f34e3f28c69018c0cc765048f975175cfea7cf9875c2b0fb  LokaTutorialScenarioWin32.exe
a7ba31879abff151b4a9708adb74e645d30d77f2bdbcab05f1bfe744732926d4  LokaFloppyBirdScenarioWin32.exe
```

## Why this lives here and not in the tracked tree

Rig-local goldens are untracked: they depend on the machine's fonts and chrome.
Reviewers see the captures through this evidence branch instead.
