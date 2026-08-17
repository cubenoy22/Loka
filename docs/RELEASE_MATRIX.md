# Release verification matrix

This document is the release-PR checklist for the verification design frozen in
[#311](https://github.com/cubenoy22/Loka/issues/311). Replace each applicable
empty evidence slot with a link from the release PR; do not mark a cell complete
without that evidence. `n/a` means that the repository has no application target
for that leg, not that verification was skipped.

For capture-bearing grades (L1 and L2), ✅ means that both layers passed: the
durable scenario audit matched its tracked expected audit byte-for-byte, and the
settled pixels matched that rig's untracked golden. The expected audit is the
cross-platform structural authority. Its verdict body is serialized by
[`common/testing/snap/SnapFormat.hpp`](../common/testing/snap/SnapFormat.hpp),
but runners treat the complete audit as opaque bytes: launch, collect, `cmp`,
then compare pixels. Pixel goldens remain local to each rig because system fonts
and chrome vary. A capture is settled only after the scenario completion marker
and consecutive identical frames.

The release gate is:

- L0 + L1: every applicable cell is automated and passes.
- L2: one representative scenario per example passes on every applicable OS.
- L3: one example cell is sampled on each applicable OS for every release.

Before applying the matrix, complete the release-provenance check:

- [ ] Every attached archive and adjacent content manifest was produced by
  `scripts/release/assemble.py` at the release tag from an explicit allowlist;
  each listed file's hash and provenance was reviewed, and each listed
  `.LRP`/`.LRPK` passed the tracked-package-file gate plus the manual
  tracked-input review documented in `scripts/release/README.md`.
- [ ] The top-level CMake source version, tag name, current-release
  documentation, and GitHub release metadata agree; `git cat-file -t <tag>`
  reports `tag` (an annotated tag, not `commit`); the annotation contains the
  release notes; and every cited issue or pull request was verified as shipped.
  The annotated-tag requirement applies to releases after v0.0.2; the published
  v0.0.2 lightweight tag is grandfathered and stays as it is.

The OS columns come from `CMakePresets.json` and `.github/workflows/ci.yml`.
Linux is a headless host-validation leg and has no GUI example targets. The
Classic Mac column is one OS leg: L0 covers both configured Retro68 architectures
(68K and PPC), while L1-L3 use the available 68K MAME or hardware rig. The current
tree gives `ScrapbookUI` Win32 and macOS target arms;
[#281](https://github.com/cubenoy22/Loka/issues/281) remains relevant to its full
cross-platform verification, and the missing native ScrollBar arms tracked by
[#224](https://github.com/cubenoy22/Loka/issues/224) do not make the example
inapplicable because its shared UI uses Previous/Next buttons.

## Grades

| Grade | Meaning | Automation |
| --- | --- | --- |
| L0 build | Every example compiles for every applicable OS/architecture. | Existing Win32 and macOS CI application builds, plus local Retro68 builds. Linux CI validates host code but contributes no example cells. |
| L1 startup smoke | Launch the example's scene and capture its settled initial screen. Classic, macOS, and Win32 cells run the example's `MainNode` inside a TEST scenario vehicle (shipping binaries carry no audit door), while that vehicle forwards the example-owned production window chrome and menus. Host parity pins compose the same platform-neutral presentation classes used by every rail so those declarations cannot drift unnoticed. | Classic MAME, Win32, and macOS startup paths exist for `ScrapbookUI`, `HelloWorld`, `Tutorial`, `MineSweeper`, and `FloppyBird`; the remaining runners are tracked by [#312](https://github.com/cubenoy22/Loka/issues/312). |
| L2 scenario completion | Drive a representative Flow/State operation sequence to completion and capture its checkpoints. | Direct Flow/State emission, shared across OS runners. Classic, Win32, and macOS runners cover `ScrapbookUI`, `HelloWorld`, `Tutorial`, `MineSweeper`, and `FloppyBird`; expansion is tracked by [#312](https://github.com/cubenoy22/Loka/issues/312). |
| L3 real hardware / manual | Exercise hands-on behavior and input feel on a real or manually operated target. | Deliberately manual; real input synthesis belongs here and in input-path PR acceptance, not in the standing L2 release gate. |

## L0 — build

| Example | Linux host | Win32 | macOS | Classic Mac (Retro68) |
| --- | --- | --- | --- | --- |
| `FloppyBird` | n/a — no Linux GUI application target | - [x] ✅ [CI at RC-equivalent `c97df048`](https://github.com/cubenoy22/Loka/actions/runs/31988978129) | - [x] ✅ [CI at RC-equivalent `c97df048`](https://github.com/cubenoy22/Loka/actions/runs/31988978129) | 68K: - [x] ✅ [Classic RC sweep](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/classic-host-rc-d6a44b5f.zip) (`68k-build.log`; 193/193, all six release bins + `ASSETS.LRP` present)<br>PPC: - [x] ✅ [Classic RC sweep](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/classic-host-rc-d6a44b5f.zip) (`ppc-build.log`; 84/84, build-verified only) |
| `HelloWorld` | n/a — no Linux GUI application target | - [x] ✅ [CI at RC-equivalent `c97df048`](https://github.com/cubenoy22/Loka/actions/runs/31988978129) | - [x] ✅ [CI at RC-equivalent `c97df048`](https://github.com/cubenoy22/Loka/actions/runs/31988978129) | 68K: - [x] ✅ [Classic RC sweep](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/classic-host-rc-d6a44b5f.zip) (`68k-build.log`; 193/193, all six release bins + `ASSETS.LRP` present)<br>PPC: - [x] ✅ [Classic RC sweep](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/classic-host-rc-d6a44b5f.zip) (`ppc-build.log`; 84/84, build-verified only) |
| `MineSweeper` | n/a — no Linux GUI application target | - [x] ✅ [CI at RC-equivalent `c97df048`](https://github.com/cubenoy22/Loka/actions/runs/31988978129) | - [x] ✅ [CI at RC-equivalent `c97df048`](https://github.com/cubenoy22/Loka/actions/runs/31988978129) | 68K: - [x] ✅ [Classic RC sweep](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/classic-host-rc-d6a44b5f.zip) (`68k-build.log`; 193/193, all six release bins + `ASSETS.LRP` present)<br>PPC: - [x] ✅ [Classic RC sweep](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/classic-host-rc-d6a44b5f.zip) (`ppc-build.log`; 84/84, build-verified only) |
| `ScrapbookUI` | n/a — no Linux GUI application target | - [x] ✅ [CI at RC-equivalent `c97df048`](https://github.com/cubenoy22/Loka/actions/runs/31988978129) | - [x] ✅ [CI at RC-equivalent `c97df048`](https://github.com/cubenoy22/Loka/actions/runs/31988978129) | 68K: - [x] ✅ [Classic RC sweep](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/classic-host-rc-d6a44b5f.zip) (`68k-build.log`; 193/193, all six release bins + `ASSETS.LRP` present)<br>PPC: n/a — its CMake target is Retro68/68K-only |
| `SimpleViewer` | n/a — no Linux GUI application target | - [x] ✅ [CI at RC-equivalent `c97df048`](https://github.com/cubenoy22/Loka/actions/runs/31988978129) | - [x] ✅ [CI at RC-equivalent `c97df048`](https://github.com/cubenoy22/Loka/actions/runs/31988978129) | 68K: - [x] ✅ [Classic RC sweep](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/classic-host-rc-d6a44b5f.zip) (`68k-build.log`; 193/193, all six release bins + `ASSETS.LRP` present)<br>PPC: - [x] ✅ [Classic RC sweep](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/classic-host-rc-d6a44b5f.zip) (`ppc-build.log`; 84/84, build-verified only) |
| `Tutorial` | n/a — no Linux GUI application target | - [x] ✅ [CI at RC-equivalent `c97df048`](https://github.com/cubenoy22/Loka/actions/runs/31988978129) | - [x] ✅ [CI at RC-equivalent `c97df048`](https://github.com/cubenoy22/Loka/actions/runs/31988978129) | 68K: - [x] ✅ [Classic RC sweep](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/classic-host-rc-d6a44b5f.zip) (`68k-build.log`; 193/193, all six release bins + `ASSETS.LRP` present)<br>PPC: - [x] ✅ [Classic RC sweep](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/classic-host-rc-d6a44b5f.zip) (`ppc-build.log`; 84/84, build-verified only) |

The Win32 and macOS L0 cells cite CI run `31988978129`, which is green at main
`c97df048`. The release candidate `d6a44b5f` differs from that commit only by the
untracked-path allowlist file `scripts/release/release-v0.0.3-classic.txt`; there
is no code delta, so that CI build is RC-equivalent. The Classic cells come from a
clean local rebuild at the RC itself; the PPC leg is build-verified only, per the
column note above.

## L1 — startup smoke

| Example | Linux host | Win32 | macOS | Classic Mac (68K rig) |
| --- | --- | --- | --- | --- |
| `FloppyBird` | n/a — no Linux GUI application target | - [x] ✅ [Win32 rail, PR #389 @ `4c7ee2db`](https://github.com/cubenoy22/Loka/pull/389) (`tests/win32/run-scenario.ps1 floppybird startup`; RC-equivalent, see the note below) | - [x] ✅ [macOS RC rail](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/rc-d6a44b5f-macos.tar.gz) (`tests/macos/run-scenario.sh floppybird startup`; log `logs/floppybird__startup.verify.log`) | - [x] ✅ [Classic RC sweep](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/classic-host-rc-d6a44b5f.zip) (`tests/toolbox/run-scenario.sh floppybird startup`; log `floppybird-startup.log`) |
| `HelloWorld` | n/a — no Linux GUI application target | - [x] ✅ [Win32 rail, PR #389 @ `4c7ee2db`](https://github.com/cubenoy22/Loka/pull/389) (`tests/win32/run-scenario.ps1 helloworld startup`; RC-equivalent, see the note below) | - [x] ✅ [macOS RC rail](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/rc-d6a44b5f-macos.tar.gz) (`tests/macos/run-scenario.sh helloworld startup`; log `logs/helloworld__startup.verify.log`) | - [x] ✅ [Classic RC sweep](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/classic-host-rc-d6a44b5f.zip) (`tests/toolbox/run-scenario.sh helloworld startup`; log `helloworld-startup.verify2.log`, run after the rig-local golden was re-baselined for the desktop-chrome diff tracked by [#408](https://github.com/cubenoy22/Loka/issues/408)) |
| `MineSweeper` | n/a — no Linux GUI application target | - [x] ✅ [Win32 rail, PR #389 @ `4c7ee2db`](https://github.com/cubenoy22/Loka/pull/389) (`tests/win32/run-scenario.ps1 minesweeper startup`; RC-equivalent, see the note below) | - [x] ✅ [macOS RC rail](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/rc-d6a44b5f-macos.tar.gz) (`tests/macos/run-scenario.sh minesweeper startup`; log `logs/minesweeper__startup.verify.log`) | - [x] ✅ [Classic RC sweep](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/classic-host-rc-d6a44b5f.zip) (`tests/toolbox/run-scenario.sh minesweeper startup`; log `minesweeper-startup.log`) |
| `ScrapbookUI` | n/a — no Linux GUI application target | - [x] ✅ [Win32 rail, PR #389 @ `4c7ee2db`](https://github.com/cubenoy22/Loka/pull/389) (`tests/win32/run-scenario.ps1 scrapbook startup`; RC-equivalent, see the note below) | - [x] ✅ [macOS RC rail](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/rc-d6a44b5f-macos.tar.gz) (`tests/macos/run-scenario.sh scrapbook startup`; log `logs/scrapbook__startup.verify.log`) | - [x] ✅ [Classic RC sweep](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/classic-host-rc-d6a44b5f.zip) (`open-first-page`; log `scrapbook-open-first-page.log`) |
| `SimpleViewer` | n/a — no Linux GUI application target | - [ ] Evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** |
| `Tutorial` | n/a — no Linux GUI application target | - [x] ✅ [Win32 rail, PR #389 @ `4c7ee2db`](https://github.com/cubenoy22/Loka/pull/389) (`tests/win32/run-scenario.ps1 tutorial startup`; RC-equivalent, see the note below) | - [x] ✅ [macOS RC rail](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/rc-d6a44b5f-macos.tar.gz) (`tests/macos/run-scenario.sh tutorial startup`; log `logs/tutorial__startup.verify.log`) | - [x] ✅ [Classic RC sweep](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/classic-host-rc-d6a44b5f.zip) (`tests/toolbox/run-scenario.sh tutorial startup`; log `tutorial-startup.log`) |

The Win32 cells cite PR [#389](https://github.com/cubenoy22/Loka/pull/389) head
`4c7ee2db` (base `ca6fa55a`), a full 17/17 update-and-verify matrix on real
Windows ARM64. The delta from that base to the RC is
[#403](https://github.com/cubenoy22/Loka/pull/403) — documentation and debug-rig
scripts only, no Win32-visible source — so those runs are binary-equivalent RC
evidence.

The macOS rail ran 14/17 at the RC. The three red cells
(`scrapbook open-first-page-refused`, `scrapbook refused-flip-keeps-page`,
`scrapbook open-text-page-refused`) fail because the macOS runner lacks the
corrupt-bag staging the refusal scenarios need; that is a runner gap tracked as
[#407](https://github.com/cubenoy22/Loka/issues/407), and the same refusal family
is witnessed green on the Classic rail at the same RC. No cell named in this
matrix is affected.

## L2 — scenario completion

Record the chosen scenario name as well as its evidence. A release needs one
representative scenario for each example on every applicable OS.

| Example | Linux host | Win32 | macOS | Classic Mac (68K rig) |
| --- | --- | --- | --- | --- |
| `FloppyBird` | n/a — no Linux GUI application target | - [x] ✅ Scenario: `fixed-step-flaps`; evidence: [Win32 rail, PR #389 @ `4c7ee2db`](https://github.com/cubenoy22/Loka/pull/389) (`tests/win32/run-scenario.ps1 floppybird fixed-step-flaps`; RC-equivalent, see the note below) | - [x] ✅ Scenario: `fixed-step-flaps`; evidence: [macOS RC rail](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/rc-d6a44b5f-macos.tar.gz) (`tests/macos/run-scenario.sh floppybird fixed-step-flaps`; log `logs/floppybird__fixed-step-flaps.verify.log`) | - [x] ✅ Scenario: `fixed-step-flaps`; evidence: [Classic RC sweep](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/classic-host-rc-d6a44b5f.zip) (`tests/toolbox/run-scenario.sh floppybird fixed-step-flaps`; log `floppybird-fixed-step-flaps.log`) |
| `HelloWorld` | n/a — no Linux GUI application target | - [x] ✅ Scenario: `toggle-action-probe`; evidence: [Win32 rail, PR #389 @ `4c7ee2db`](https://github.com/cubenoy22/Loka/pull/389) (`tests/win32/run-scenario.ps1 helloworld toggle-action-probe`; RC-equivalent, see the note below) | - [x] ✅ Scenario: `toggle-action-probe`; evidence: [macOS RC rail](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/rc-d6a44b5f-macos.tar.gz) (`tests/macos/run-scenario.sh helloworld toggle-action-probe`; log `logs/helloworld__toggle-action-probe.verify.log`) | - [x] ✅ Scenario: `toggle-action-probe`; evidence: [Classic RC sweep](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/classic-host-rc-d6a44b5f.zip) (`tests/toolbox/run-scenario.sh helloworld toggle-action-probe`; log `helloworld-toggle-action-probe.verify2.log`, run after the [#408](https://github.com/cubenoy22/Loka/issues/408) golden re-baseline) |
| `MineSweeper` | n/a — no Linux GUI application target | - [x] ✅ Scenario: `new-game-twice`; evidence: [Win32 rail, PR #389 @ `4c7ee2db`](https://github.com/cubenoy22/Loka/pull/389) (`tests/win32/run-scenario.ps1 minesweeper new-game-twice`; RC-equivalent, see the note below) | - [x] ✅ Scenario: `new-game-twice`; evidence: [macOS RC rail](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/rc-d6a44b5f-macos.tar.gz) (`tests/macos/run-scenario.sh minesweeper new-game-twice`; log `logs/minesweeper__new-game-twice.verify.log`) | - [x] ✅ Scenario: `new-game-twice`; evidence: [Classic RC sweep](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/classic-host-rc-d6a44b5f.zip) (`tests/toolbox/run-scenario.sh minesweeper new-game-twice`; log `minesweeper-new-game-twice.log`) |
| `ScrapbookUI` | n/a — no Linux GUI application target | - [x] ✅ Scenario: `flip-forward-back`; evidence: [Win32 rail, PR #389 @ `4c7ee2db`](https://github.com/cubenoy22/Loka/pull/389) (`tests/win32/run-scenario.ps1 scrapbook flip-forward-back`; RC-equivalent, see the note below) | - [x] ✅ Scenario: `flip-forward-back`; evidence: [macOS RC rail](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/rc-d6a44b5f-macos.tar.gz) (`tests/macos/run-scenario.sh scrapbook flip-forward-back`; log `logs/scrapbook__flip-forward-back.verify.log`) | - [x] ✅ Scenario: `flip-forward-back`; evidence: [Classic RC sweep](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/classic-host-rc-d6a44b5f.zip) (`tests/toolbox/run-scenario.sh scrapbook flip-forward-back`; log `scrapbook-flip-forward-back.log`) |
| `SimpleViewer` | n/a — no Linux GUI application target | - [ ] Scenario: _add name_; evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Scenario: _add name_; evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Scenario: _add name_; evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** |
| `Tutorial` | n/a — no Linux GUI application target | - [x] ✅ Scenario: `increment-summary-toggle`; evidence: [Win32 rail, PR #389 @ `4c7ee2db`](https://github.com/cubenoy22/Loka/pull/389) (`tests/win32/run-scenario.ps1 tutorial increment-summary-toggle`; RC-equivalent, see the note below) | - [x] ✅ Scenario: `increment-summary-toggle`; evidence: [macOS RC rail](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/rc-d6a44b5f-macos.tar.gz) (`tests/macos/run-scenario.sh tutorial increment-summary-toggle`; log `logs/tutorial__increment-summary-toggle.verify.log`) | - [x] ✅ Scenario: `increment-summary-toggle`; evidence: [Classic RC sweep](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/classic-host-rc-d6a44b5f.zip) (`tests/toolbox/run-scenario.sh tutorial increment-summary-toggle`; log `tutorial-increment-summary-toggle.log`) |

The Win32 and macOS notes under the L1 table apply here as well: the Win32 cells
are PR [#389](https://github.com/cubenoy22/Loka/pull/389) head `4c7ee2db` runs
that are binary-equivalent RC evidence, and the macOS
[#407](https://github.com/cubenoy22/Loka/issues/407) runner gap touches only
scrapbook refusal cells, none of which this table names. The Classic refusal
cells (`open-first-page-refused`, `refused-flip-keeps-page`,
`open-text-page-refused`) all pass at the RC in the
[Classic RC sweep](https://raw.githubusercontent.com/cubenoy22/Loka/pr-assets/v0.0.3-rc-d6a44b5f/classic-host-rc-d6a44b5f.zip),
which is 17/17 overall; the host leg in the same archive is 399/399
(`host-ctest.log`).

## L3 — real hardware / manual

Select and complete one example cell per applicable OS for this release. Leave
the other cells unchecked so the sample remains visible.

| Example | Linux host | Win32 | macOS | Classic Mac |
| --- | --- | --- | --- | --- |
| `FloppyBird` | n/a — no Linux GUI application target | - [ ] Sample evidence: _add link_ | - [ ] Sample evidence: _add link_ | - [ ] Sample evidence: _add link_ |
| `HelloWorld` | n/a — no Linux GUI application target | - [ ] Sample evidence: _add link_ | - [ ] Sample evidence: _add link_ | - [ ] Sample evidence: _add link_ |
| `MineSweeper` | n/a — no Linux GUI application target | - [ ] Sample evidence: _add link_ | - [ ] Sample evidence: _add link_ | - [ ] Sample evidence: _add link_ |
| `ScrapbookUI` | n/a — no Linux GUI application target | - [ ] Sample evidence: _add link_ | - [ ] Sample evidence: _add link_ | - [ ] Sample evidence: _add link_ |
| `SimpleViewer` | n/a — no Linux GUI application target | - [ ] Sample evidence: _add link_ | - [ ] Sample evidence: _add link_ | - [ ] Sample evidence: _add link_ |
| `Tutorial` | n/a — no Linux GUI application target | - [ ] Sample evidence: _add link_ | - [ ] Sample evidence: _add link_ | - [ ] Sample evidence: _add link_ |

## How to re-run

Run commands from the repository root. The configure-and-build lines below are
single shell invocations that reproduce each build leg available today; the
host must satisfy that preset's toolchain requirements.

| Grade / leg | One-command invocation available today |
| --- | --- |
| L0 Linux host | `cmake --preset testing && cmake --build --preset testing` (host validation only; there are no Linux GUI example cells) |
| L0 Win32 | `cmake --preset win32-debug && cmake --build --preset win32-debug` |
| L0 macOS | `cmake --preset macos-debug && cmake --build --preset macos-debug` |
| L0 Classic Mac 68K | `cmake --preset retro68-68k-release && cmake --build --preset retro68-68k-release` |
| L0 Classic Mac PPC | `cmake --preset retro68-ppc-release && cmake --build --preset retro68-ppc-release` |
| L1 Classic `ScrapbookUI` | `tests/toolbox/run-scenario.sh scrapbook startup` — tracked expected audit plus settled rig-local pixel golden |
| L1 Classic `HelloWorld` | `tests/toolbox/run-scenario.sh helloworld startup` — settled initial title observation, tracked expected audit, and settled rig-local pixel golden |
| L1 Classic `Tutorial` | `tests/toolbox/run-scenario.sh tutorial startup` — settled initial tutorial title observation, tracked expected audit, and settled rig-local pixel golden |
| L1 Classic `MineSweeper` | `tests/toolbox/run-scenario.sh minesweeper startup` — settled initial New Game control observation, tracked expected audit, and settled rig-local pixel golden |
| L1 Classic `FloppyBird` | `tests/toolbox/run-scenario.sh floppybird startup` — fixed-step initial surface observation, tracked expected audit, and settled rig-local pixel golden |
| L2 Classic `ScrapbookUI` | `tests/toolbox/run-scenario.sh scrapbook flip-forward-back` — tracked expected audit plus settled rig-local pixel golden |
| L2 Classic `HelloWorld` | `tests/toolbox/run-scenario.sh helloworld toggle-action-probe` — typed TEST_ID actions drive MainNode-owned Emitters; tracked expected audit plus settled rig-local pixel golden |
| L2 Classic `Tutorial` | `tests/toolbox/run-scenario.sh tutorial increment-summary-toggle` — typed TEST_ID actions increment Step 4 twice, hide and restore its derived summary, and pin the full audit plus settled rig-local pixel golden; EditText remains outside Tutorial's runtime path ([#167](https://github.com/cubenoy22/Loka/issues/167)) |
| L2 Classic `MineSweeper` | `tests/toolbox/run-scenario.sh minesweeper new-game-twice` — fixed caller-owned seed pins the initial board and both MainNode-owned New Game commands; tracked expected audit plus settled rig-local pixel golden |
| L2 Classic `FloppyBird` | `tests/toolbox/run-scenario.sh floppybird fixed-step-flaps` — fixed caller-owned seed and exact 1/60-step advancement pin five flaps and surface checkpoints; tracked expected audit plus settled rig-local pixel golden |
| Standalone Classic `Tutorial` | `LokaTutorialStandaloneFlow68K_APPL` presents the same typed scenario without host config; after target execution, `tests/toolbox/verify-standalone-audit.sh tutorial increment-summary-toggle <LOG.TXT>` byte-compares the complete durable audit |
| Standalone Classic `MineSweeper` | `LokaMineStandaloneFlow68K_APPL` presents the fixed-seed two-New-Game tour without host config; after target execution, `tests/toolbox/verify-standalone-audit.sh minesweeper new-game-twice <LOG.TXT>` byte-compares the complete durable audit |
| Standalone Classic `FloppyBird` | `LokaFloppyStandaloneFlow68K_APPL` presents the fixed-seed fixed-step flap tour without host config; after target execution, `tests/toolbox/verify-standalone-audit.sh floppybird fixed-step-flaps <LOG.TXT>` byte-compares the complete durable audit |
| L1 Win32 `ScrapbookUI` | `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/win32/run-scenario.ps1 scrapbook startup` — shared State-driven scenario, byte-identical tracked audit, two-hash settled `PrintWindow` capture, and rig-local profiled golden |
| L2 Win32 `ScrapbookUI` | `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/win32/run-scenario.ps1 scrapbook flip-forward-back` — shared State-driven scenario, byte-identical tracked audit, two-hash settled `PrintWindow` capture, and rig-local profiled golden |
| L1/L2 Win32 other scenario examples | `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/win32/run-scenario.ps1 <example> <scenario>` — shared scene driver, byte-identical tracked audit, two-hash settled `PrintWindow` capture, and rig-local profiled golden |
| L1/L2 macOS | `tests/macos/run-scenario.sh <example> <scenario>` — tracked expected audit plus settled rig-local pixel golden |
| L1 Classic `SimpleViewer` | **TBD — [#312](https://github.com/cubenoy22/Loka/issues/312)** |
| L2 Classic examples other than `ScrapbookUI`, `HelloWorld`, `Tutorial`, `MineSweeper`, and `FloppyBird` | **TBD — [#312](https://github.com/cubenoy22/Loka/issues/312)** |
| L3 all OSes | n/a — manual by definition; record the rig/hardware and evidence in the selected matrix cell |

The Classic scenario command requires an already configured local MAME rig,
the example's Toolbox test application, and the host `lrpc` tool; if an
artifact is absent, the runner prints its exact build command.
`tests/scenarios/scenarios.txt` is the shared `<example> <scenario>` registry.
`--update-golden` regenerates the rig-local golden under that same per-example
layout; it does not create release evidence or a tracked repository file.
