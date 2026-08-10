# Release verification matrix

This document is the release-PR checklist for the verification design frozen in
[#311](https://github.com/cubenoy22/Loka/issues/311). Replace each applicable
empty evidence slot with a link from the release PR; do not mark a cell complete
without that evidence. `n/a` means that the repository has no application target
for that leg, not that verification was skipped.

For capture-bearing grades (L1 and L2), ✅ means that both layers passed: the
deterministic SnapRecord matched, and the rig-local cropped pixels matched that
rig's untracked golden. SnapRecord is the cross-platform authority; its V1 schema
is defined by
[`common/testing/snap/SnapFormat.hpp`](../common/testing/snap/SnapFormat.hpp).
Pixel goldens remain local to each rig because system fonts and chrome vary. A
capture is settled only after the scenario completion marker and two consecutive
frames with the same hash; the pixel comparison is limited to the SnapRecord crop
bounds.

Today's Classic runner satisfies only part of this contract: its verdict parses
`status` and the crop fields without comparing the rest of the SnapRecord to a
tracked expected record, and its capture waits a fixed interval for a single
snapshot instead of the marker-plus-two-identical-hashes rule. Until
[#314](https://github.com/cubenoy22/Loka/issues/314) upgrades it, a Classic ✅
requires attaching the SnapRecord to the evidence link and reviewing it by hand.

The release gate is:

- L0 + L1: every applicable cell is automated and passes.
- L2: one representative scenario per example passes on every applicable OS.
- L3: one example cell is sampled on each applicable OS for every release.

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
| L1 startup smoke | Launch the example and capture its settled initial screen. | Intended for every applicable cell; MAME is present for one Classic `ScrapbookUI` startup path, while the remaining runners are tracked by [#312](https://github.com/cubenoy22/Loka/issues/312). |
| L2 scenario completion | Drive a representative Flow/State operation sequence to completion and capture its checkpoints. | Direct Flow/State emission, shared across OS runners. Only the Classic `ScrapbookUI` runner exists today; expansion is tracked by [#312](https://github.com/cubenoy22/Loka/issues/312). |
| L3 real hardware / manual | Exercise hands-on behavior and input feel on a real or manually operated target. | Deliberately manual; real input synthesis belongs here and in input-path PR acceptance, not in the standing L2 release gate. |

## L0 — build

| Example | Linux host | Win32 | macOS | Classic Mac (Retro68) |
| --- | --- | --- | --- | --- |
| `FloppyBird` | n/a — no Linux GUI application target | - [ ] Evidence: _add link_ | - [ ] Evidence: _add link_ | 68K: - [ ] Evidence: _add link_<br>PPC: - [ ] Evidence: _add link_ |
| `HelloWorld` | n/a — no Linux GUI application target | - [ ] Evidence: _add link_ | - [ ] Evidence: _add link_ | 68K: - [ ] Evidence: _add link_<br>PPC: - [ ] Evidence: _add link_ |
| `MineSweeper` | n/a — no Linux GUI application target | - [ ] Evidence: _add link_ | - [ ] Evidence: _add link_ | 68K: - [ ] Evidence: _add link_<br>PPC: - [ ] Evidence: _add link_ |
| `ScrapbookUI` | n/a — no Linux GUI application target | - [ ] Evidence: _add link_ | - [ ] Evidence: _add link_ | 68K: - [ ] Evidence: _add link_<br>PPC: n/a — its CMake target is Retro68/68K-only |
| `SimpleViewer` | n/a — no Linux GUI application target | - [ ] Evidence: _add link_ | - [ ] Evidence: _add link_ | 68K: - [ ] Evidence: _add link_<br>PPC: - [ ] Evidence: _add link_ |
| `Tutorial` | n/a — no Linux GUI application target | - [ ] Evidence: _add link_ | - [ ] Evidence: _add link_ | 68K: - [ ] Evidence: _add link_<br>PPC: - [ ] Evidence: _add link_ |

## L1 — startup smoke

| Example | Linux host | Win32 | macOS | Classic Mac (68K rig) |
| --- | --- | --- | --- | --- |
| `FloppyBird` | n/a — no Linux GUI application target | - [ ] Evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** |
| `HelloWorld` | n/a — no Linux GUI application target | - [ ] Evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** |
| `MineSweeper` | n/a — no Linux GUI application target | - [ ] Evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** |
| `ScrapbookUI` | n/a — no Linux GUI application target | - [ ] Evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Evidence: _add link_ (`open-first-page`) |
| `SimpleViewer` | n/a — no Linux GUI application target | - [ ] Evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** |
| `Tutorial` | n/a — no Linux GUI application target | - [ ] Evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** |

## L2 — scenario completion

Record the chosen scenario name as well as its evidence. A release needs one
representative scenario for each example on every applicable OS.

| Example | Linux host | Win32 | macOS | Classic Mac (68K rig) |
| --- | --- | --- | --- | --- |
| `FloppyBird` | n/a — no Linux GUI application target | - [ ] Scenario: _add name_; evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Scenario: _add name_; evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Scenario: _add name_; evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** |
| `HelloWorld` | n/a — no Linux GUI application target | - [ ] Scenario: _add name_; evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Scenario: _add name_; evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Scenario: _add name_; evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** |
| `MineSweeper` | n/a — no Linux GUI application target | - [ ] Scenario: _add name_; evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Scenario: _add name_; evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Scenario: _add name_; evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** |
| `ScrapbookUI` | n/a — no Linux GUI application target | - [ ] Scenario: _add name_; evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Scenario: _add name_; evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Scenario: _add name_; evidence: _add link_ |
| `SimpleViewer` | n/a — no Linux GUI application target | - [ ] Scenario: _add name_; evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Scenario: _add name_; evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Scenario: _add name_; evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** |
| `Tutorial` | n/a — no Linux GUI application target | - [ ] Scenario: _add name_; evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Scenario: _add name_; evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** | - [ ] Scenario: _add name_; evidence: _add link_; runner **TBD ([#312](https://github.com/cubenoy22/Loka/issues/312))** |

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
| L1 Classic `ScrapbookUI` | `tests/toolbox/run-scenario.sh open-first-page` — status/crop verdict and fixed-wait single snapshot today; full-record comparison and settled capture are tracked in [#314](https://github.com/cubenoy22/Loka/issues/314) |
| L2 Classic `ScrapbookUI` | `tests/toolbox/run-scenario.sh <scenario>` — same [#314](https://github.com/cubenoy22/Loka/issues/314) caveat |
| L1/L2 Win32 | **TBD — [#312](https://github.com/cubenoy22/Loka/issues/312)** |
| L1/L2 macOS | **TBD — [#312](https://github.com/cubenoy22/Loka/issues/312)** |
| L1/L2 Classic examples other than `ScrapbookUI` | **TBD — [#312](https://github.com/cubenoy22/Loka/issues/312)** |
| L3 all OSes | n/a — manual by definition; record the rig/hardware and evidence in the selected matrix cell |

The Classic scenario command requires an already configured local MAME rig and
the `LokaTestsToolbox68K_APPL` build artifact; if the artifact is absent, the
runner prints the exact Retro68 build command. Its accepted scenario names today
are `open-first-page`, `open-first-page-refused`, `flip-forward-back`,
`refused-flip-keeps-page`, `open-text-page`, and
`open-text-page-refused`. `--update-golden` regenerates the rig-local golden; it
does not create release evidence or a tracked repository file.
