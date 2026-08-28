# Classic (MAME maciix) goldens — 2026-08-26 generation

The 0.0.4 set. Follows `classic-maciix-2026-08-22/` (the 0.0.3a re-bake, cut at
`main = 905eba6a`).

Baked on **Omen** (x86_64 Win11) at `main = a36148fa`, all 16 registered cells.
`manifest.txt` travels with the set and is the authority; this file is the story
around it.

## Contents

16 scenarios across 5 examples, plus `manifest.txt` and `SHA256SUMS`.

scrapbook 6 / helloworld 3 / minesweeper 3 / tutorial 2 / floppybird 2

Same cell set as the 2026-08-22 generation.

## What this set is, and is not

It records **what this version drew in this environment**. It is not a claim that
the pixels are correct. Known drawing defects present in these captures:

- **#45 item 7** — wrap-mode measurement diverges across rails, silently truncating
  a label. Classic is the rail that gets it right, so this set does not show it,
  but the same cells on macOS and Win32 do.
- **#481** — the `*` overlapping `Loka Sample` in HelloWorld is the example's
  deliberate ZStack overlay demonstration, not a drawing defect (clarified by
  the maintainer 2026-08-28); it is a legibility question, not correctness.

## Version-to-version comparison against 2026-08-22

The previous generation was restored and compared before this one was baked.
Per the golden-lifetime ruling, a differing cell requires an explanation rather
than a re-bake.

| Layer | Result |
| --- | --- |
| Tracked audit (the machine verdict) | **16/16 byte-identical** |
| Pixels | **14/16 byte-identical** |

Both differing cells are `floppybird`, and both are intended:

- `fixed-step-flaps` — the scenario was extended from 192 to 273 fixed steps and
  gained `verify-game-over` / `verify-game-over-held` (#482). The **tracked
  expected audit declares this**, and the run matches that new expectation. The
  capture shows the bird fallen at game over rather than mid-flight.
- `startup` — 2 pixels at (211,51)-(212,51), from the same PR's deterministic
  game-state reset. The audit matches.

The other 14 cells, including all four scrapbook refusal cells, are unchanged.

## Identity

This rail can reproduce its environment later, which is why this set is usable as
the comparison base for the next release. `manifest.txt` carries:

```
gcc_version                16.1.0
universal_interfaces_version 0x0340
retro68_identity           07ba932b… (toolchain-content-sha256)
mame_executable_sha256     af696610…
mame_rom_identity          bdc7da90… (verified-rom-inventory-sha256)
ram_size                   8M
machine                    maciix
boot_hd_sha256             415438ed…
```

The boot template is pinned read-only on the rig with a recorded SHA-256, so the
running environment does not drift between bakes.

## Why this lives here and not in the tracked tree

The pixels contain Apple-rendered glyphs from the boot image's System resources.
The licensing rule keeps them out of the repository; reviewers see the captures
through this evidence branch instead (ruling 2026-08-18).
