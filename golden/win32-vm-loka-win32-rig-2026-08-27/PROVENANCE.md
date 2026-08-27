# Win32 (Hyper-V VM) goldens — 2026-08-27 generation, rig `loka-win32-rig`

**The first Win32 golden set in the image-backed column.** Baked on the
isolated Hyper-V guest `loka-win32-rig` at `main = 92098ee1` (post-0.0.4
aftercare). All ten registered Win32 matrix cells: five examples × L1
`startup` + L2 representative.

## This set IS a long-term baseline

Per the golden-lifetime ruling (2026-08-26), a long-term golden is valid only
where the OS environment of the snapshot can be reproduced later. This rig
meets the condition, which no bare-iron Win32 rig can:

- the machine is a VM image (Gen 2 Hyper-V guest on Omen);
- **Windows Update is disabled** (service disabled + `NoAutoUpdate` policy,
  2026-08-27);
- a named checkpoint freezes the exact environment these goldens were baked
  in: **`frozen-2026-08-27-updates-off-main-92098ee1`**, id
  `2d97198e-4977-411e-beee-40e168d695c2` (Standard checkpoint of the running
  guest, created after the bake below).

That puts this rig beside maciix (MAME) and the Mavericks VM: its goldens can
serve as version-to-version baselines. The bare-iron `omen` sets
(`win32-x64-*`) remain release evidence and explained comparison only.

## Environment

`os_build 10.0.26200.9168` (25H2) · x64 · virtual display 1024x768 @ 96 DPI ·
32bpp · light theme · `PrintWindow.PW_RENDERFULLCONTENT.v1`.

**`scale_percent=100` in the profiles is the truth on this rig** — the virtual
display really runs at 96 DPI — unlike on bare iron, where the DPI-unaware
profile records a constant (#492). The #492 class of silent environment drift
is closed here by construction: the display adapter is virtual, the desk never
moves, and the checkpoint records everything else.

## Determinism evidence

- `minesweeper startup` probed **8/8 runs onto one content hash**
  (`CB200D29…`) before any golden existed. The #496 multistable settle —
  ~1 in 3 incomplete boards on bare iron — **does not manifest on this rig**
  (recorded on #496). Both MineSweeper cells are therefore baked here, which
  no bare-iron set could do.
- Every cell was baked, then verified in the same session, then **verified
  again in a second independent pass: 10/10 green twice**. Identity pairs
  (`scrapbook`, `minesweeper`) aligned on the first bake attempt — no #493
  re-roll was needed.

## What this set is, and is not

It records **what this version drew in this environment**. Known drawing
defects in this build: **#481** (HelloWorld title glyph overpaint, all rails),
**#45 item 7** (wrap-mode truncation). #496 does not reproduce here but
remains open for the bare-iron rail.

## Producing application binaries (SHA-256)

```
fd5ea0a104392666aa6f32bed34b527d9bdce6c529aeae290be25c4b13bdb4c2  LokaScrapbookScenarioWin32.exe
66ed64b42cd8ef0a4961e1ce658464b0a5c410a0cff90b1fdffa4c7742563e67  LokaHelloWorldScenarioWin32.exe
a39af6e7c148655287dcf6276752d08d6c2e8f050170e4e170ba9bd4f9814928  LokaTutorialScenarioWin32.exe
e664f852dbc939b618cc1f3d5f272e05ef39f95926b42eff14f3a9a2c8e28850  LokaMineSweeperScenarioWin32.exe
e965468af5571ae2200a50b154418f449806f19fc4b81380005a140f9c842fbf  LokaFloppyBirdScenarioWin32.exe
```

## Why this lives here and not in the tracked tree

Rig-local goldens are untracked. Reviewers see the captures through this
evidence branch; the rig-local copies under
`build/win32-scenario/golden/loka-win32-rig/` in the guest are the operating
set, and the checkpoint preserves them with the machine.
