# Classic (MAME maciix) goldens — 2026-08-22 generation

The 0.0.3a re-bake. Replaces `classic-maciix-2026-08-17/`, which was cut at
`main = ec24a78c` (through #416) on the ARM64 rig.

Baked on **Omen** (x86_64 Win11) at `main = 905eba6a`, all 16 registered cells,
published atomically by the #422 bundle flow (`golden.incomplete` staging, then
one rename). `manifest.txt` travels with the set and is the authority; this file
is the story around it.

## Contents

16 scenarios across 5 examples, plus `manifest.txt` and `SHA256SUMS`.

scrapbook 6 / helloworld 3 / minesweeper 3 / tutorial 2 / floppybird 2

The 2026-08-17 set had 17. `scrapbook open-first-page` was removed by #457: it
ran the same driver and asserted the same predicate as `scrapbook startup`, so
it could not fail alone (#452).

## Identity

```
identity_sha256           080c51416bcac65bb9e621709e8666dbf06fef4acf9930b94ba4caf4e929b15d
scenario_registry_sha256  317ca65cf7201c866bf344046493c97a6b8268fbafec9440b25a7e2f0b357d24
scenario_count            16
```

Approved in `scripts/rig/toolbox/rigs/toolbox-maciix.ini` by #458, as a separate
review — the bake never writes its own authority. The rail reports
`Reference eligibility: eligible (tracked identity 080c5141...)` against this set.

## Producing environment

Every field below is recorded in `manifest.txt` and compared on every run; a
bundle is verdict-eligible only when its recorded identity matches the tracked
one.

| field | value |
| --- | --- |
| `gcc_version` | 16.1.0 |
| `universal_interfaces_version` | 0x0340 |
| `retro68_identity` | `07ba932b…` (toolchain content sha256) |
| `mame_executable_sha256` | `af696610…` |
| `mame_rom_identity` | `bdc7da90…` (verified ROM inventory) |
| `ram_size` / `machine` | 8M / maciix |
| `capture_adapter` | mame-screen-snapshot.v1 |
| `boot_hd_sha256` | `415438ed…` |

**The boot image is identified, and that is what changed since 2026-08-17.**
That set's weakest point was an unidentified boot HDA whose two copies had
already diverged. `C:\MAME\hda\boot-template.hda` is now pinned read-only at
`415438ed…` (1-bit depth), and #425 stopped the runner booting it in place. Its
permissions and mtime were checked unchanged before and after this bake.

## Relationship to the 2026-08-17 set

Ten of the sixteen are **byte-identical** to their 2026-08-17 counterparts:
`scrapbook` ×6 (of the 7 that existed; the seventh was deleted) and
`minesweeper` ×3 — plus every other cell's differences are accounted for:

| cells | differing px | cause |
| --- | --- | --- |
| `floppybird` ×2 | 2 | y51 only — the Finder's capacity readout, not Loka's drawing |
| `tutorial` ×2 | 40 | y51-57, same |
| `helloworld startup` | 9,886 | #454 moved the BMI section to the right column and grew the window 420×300 → 420×330; y371-402 is the window's bottom edge moving |
| `helloworld toggle-action-probe`, `bmi-roundtrip` | 10,478 / 10,418 | the above, plus y86-94 from #412, which landed after the 2026-08-17 set was cut |

No unexplained pixels. The ten byte-identical cells hold across **GCC 12.2.0 →
16.1.0** and across the move from the ARM64 rig to Omen — the "different
compiler, different pixels" risk #422 was built to detect did not materialise
in this range.

## Known limitation

#459: on the **Win32** rail, `scrapbook open-text-page-refused` settles to a
different single column roughly once in twenty runs, because a late repaint
changes a correct rendering and the runner's two-equal-frames settle rule
accepts the changed state. That was observed on Win32 only; this Classic bake
reproduced every cell's previous value exactly. It is recorded here because the
same settle rule governs this rail too.

## Not covered

macOS — Omen cannot bake it. #456 carries that rail.
