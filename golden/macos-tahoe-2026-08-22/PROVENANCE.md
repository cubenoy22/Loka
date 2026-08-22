# macOS goldens — 2026-08-22 generation

The 0.0.3a re-bake's macOS third, taken on **tahoe** at `main = ba6ef1e6`.
Alongside `classic-maciix-2026-08-22/` and `win32-x64-2026-08-22/`, which were
baked on Omen at `main = 905eba6a` — the two commits between them (#460, #461)
touch the Win32 rail and its tests only, and leave the scenario registry digest
unchanged.

## Contents

**13 cells of the 16 registered**, each with its `.profile`, plus `SHA256SUMS`.

scrapbook 3 / helloworld 3 / minesweeper 3 / tutorial 2 / floppybird 2

## The three that are missing, and why

```
scrapbook open-first-page-refused
scrapbook refused-flip-keeps-page
scrapbook open-text-page-refused
```

**#462.** `tests/macos/run-scenario.sh` never stages the corrupt-package fixture
that `tests/scenarios/scrapbook-package-fixtures.txt` declares for these three
cells. Win32 does it at `run-scenario.ps1:360` and Classic at
`run-scenario.sh:243`; the macOS runner has no reference to the registry, to
`lrpc`, or to the asset at all. The package loads intact, the scenario asks for
a refusal that never comes, and the audit correctly reports that its
expectations were not met.

These cells are not "not yet baked". They cannot pass on this rail as it
stands, and the refusal path they exist to cover is unverified on macOS.

## Producing environment

Recorded in every `.profile` and compared whole (`cmp -s`) on the next run.

```
profile_version=2
os_build=25G76
arch=x86_64
scale_percent=200
depth=24
appearance=light
capture_api=NSView.cacheDisplayInRect.v1
```

Intel MacBook Pro 2019, macOS 26.6.1. Note `scale_percent=200` — this rig is
Retina, so its captures are twice the logical size of the other rails'
(`scrapbook startup` is 680×500 here against 340×250 on Win32).

**macOS has no rig descriptor.** Like Win32 before #460, the environment is
recorded after a bake rather than declared before one, so a bake taken under a
different appearance or scale would pin the wrong value silently. #460 closed
that for Win32 only.

## A trap worth writing down

The first attempt at this bake ran against **stale bundles**. `macos-debug` and
`macos-tests` do not build the scenario `.app`s — `macos-tests` builds exactly
`LokaTestsMacOS` — and the rig's bundles were from Aug 17, predating #454 and
#457. All sixteen cells ran against five-day-old binaries; ten of them "passed"
only because their audits happened not to have changed.

`cmake --build --preset macos-scenarios` is the one that matters. And the
`.app` directory's mtime does not move when its contents are relinked: check
`Contents/MacOS/<target>`.

After the rebuild, `helloworld` ×3 went from failing on `crop_bottom` (330
expected, 300 reported — the pre-#454 window) to passing, and the ten
previously-passing cells reproduced their earlier pixels byte for byte.

## Byte-identity

Two collisions, both declared in `tests/scenarios/startup-golden-identities.txt`:

- `scrapbook/startup.png` = `scrapbook/flip-forward-back.png` (`f5d56bae…`)
- `minesweeper/startup.png` = `minesweeper/new-game-twice.png` (`647b5d36…`)

Nothing undeclared.
