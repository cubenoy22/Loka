# Win32 x64 goldens — 2026-08-22 generation

The 0.0.3a re-bake's Win32 half, taken alongside `classic-maciix-2026-08-22/`
on **Omen** at `main = 905eba6a`. All 16 registered cells, each recorded with
`tests/win32/run-scenario.ps1 <example> <scenario> --update-golden`.

## Contents

16 PNGs and their 16 `.profile` sidecars, plus `SHA256SUMS`.

scrapbook 6 / helloworld 3 / minesweeper 3 / tutorial 2 / floppybird 2

Unlike Classic there is no bundle manifest and no tracked rig descriptor: the
Win32 rail records its environment per golden in the `.profile` beside it, and
has nothing that declares the expected environment ahead of a bake. See the
pinning note below.

## Producing environment

Recorded in every `.profile`; `tests/win32/run-scenario.ps1` compares the whole
file and refuses on any field moving.

```
profile_version=2
os_build=10.0.26200
arch=x64
scale_percent=100
depth=32
appearance=light
capture_api=PrintWindow.PW_RENDERFULLCONTENT.v1
```

Toolchain: Visual Studio 2026 (`…\18\Community`), `cmake --preset win32-debug`
plus `win32-tests`. The 7 scrapbook cells additionally require a host
`build/host/lrpc/lrpc.exe`, which no preset builds; the runner names the fix
when it is missing.

**This set is pinned to `appearance=light`.** The same rig recorded
`appearance=dark` in captures taken on 2026-08-21, so the desktop theme moved
between sessions. If it moves back, every cell fails with

```
profile stage failed: capture field 'appearance' moved from 'light' to 'dark'
```

which is the #423 all-fields comparison doing its job — but it fails *after* a
bake rather than before one, so a bake taken under the wrong theme silently
pins the wrong value. That gap is what a Win32 rig descriptor would close.

## Known limitation

**#459.** `scrapbook open-text-page-refused` is not reproducible on this rail.
In 20 consecutive verification runs, one settled to `463dcdae…` instead of the
recorded `ed096d1c…` — a single column of the dithered page bitmap, 169 pixels,
with an unchanged audit. The failing run took three settle frames instead of
two, and **settle frame 1 matched the golden exactly**: the window renders
correctly, a late repaint changes one column, and the two-equal-frames rule
accepts the changed state.

The first 0.0.3a Win32 bake recorded the `463dcdae…` value. This set carries
`ed096d1c…`, which matched settle frame 1 and passed 19 of 20 runs. Any future
bake of this cell can pick up the other value.

## Not covered

macOS — #456. The three-rail set is two rails until a Mac rig runs it.
