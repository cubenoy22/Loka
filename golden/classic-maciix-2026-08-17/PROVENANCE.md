# Classic (MAME maciix) goldens — 2026-08-17 generation

Archived 2026-08-18 from `build/mame-scenario/golden/` in the human's clone,
which is git-ignored and does not survive a `build/` wipe.

## Contents

17 scenarios across 5 examples, each as a PNG plus its `.mame-machine` sidecar,
with a `SHA256SUMS` manifest.

floppybird 2 / helloworld 3 / minesweeper 3 / scrapbook 7 / tutorial 2

Repository state when archived: `main = ec24a78c` (through #416).

## Generations within this set (from file mtimes)

| Recorded | Count | What |
| --- | --- | --- |
| 2026-08-16 23:34–23:39 | 13 | One sweep |
| 2026-08-17 11:59 | 2 | `helloworld/bmi-roundtrip`, `minesweeper/seeded-reveal` — first capture |
| 2026-08-17 12:21 | 2 | `helloworld/startup`, `helloworld/toggle-action-probe` — legitimate re-baseline after #400 changed the vehicle's size and shifted the Finder header by 2px; see #408 |

All 17 are post-#400, so the set is internally consistent.

## Producing environment, as far as it is recorded

- MAME machine `maciix` (recorded per capture in the `.mame-machine` sidecars)
- RAM 8M (`MAME_RAMSIZE` in `.env-mame`)
- MAME at `C:\mame\mame.exe`, ROMs at `C:\mame\roms`

**The boot image is not identified, and that is this archive's weakest point.**
Two boot HDAs exist on the rig and their contents have already diverged:

- the original that `.env-mame` points at, on iCloud Drive (mtime 2026-08-17 22:11)
- a local pinned copy under `loka-gdb/` that the run scripts use (mtime 2026-08-11 00:30)

`cmp` reports they differ from byte 50184; both are 104,857,600 bytes. MAME writes
back to the HDA, so it changes on every boot. The System fonts and control chrome
that these pixels are made of live inside that image, so a golden mismatch cannot
currently be attributed to either a Loka regression or a boot image change. Adding
a boot image fingerprint is tracked on the Classic rig-profile card.

## Scattered copies that were not adopted

| Location | Count | Verdict |
| --- | --- | --- |
| `loka-release-003` worktree | 15 | Older generation: no sidecars, pixels differ, `helloworld/bmi-roundtrip` and `minesweeper/seeded-reveal` missing entirely |
| fleet clone | 6 | Older layout: no per-example subdirectories, scrapbook only |

None of the three sets can be told apart from their files alone. That is the
concrete instance of the "goldens have no identity" problem.

## Verifying

```sh
sha256sum -c SHA256SUMS
```
