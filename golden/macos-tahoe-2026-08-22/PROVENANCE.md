# macOS goldens — 2026-08-22 generation

The 0.0.3a re-bake's macOS third, taken on **tahoe**. Thirteen cells at
`main = ba6ef1e6`; the three Scrapbook refusal cells at `main = ad8c8f08`, once
#467 gave this rail the fixture staging it never had. The thirteen were
re-compared against the rig at `ad8c8f08` before the three were added and are
byte-identical, so the set is one generation despite the two commits.
Alongside `classic-maciix-2026-08-22/` and `win32-x64-2026-08-22/`, which were
baked on Omen at `main = 905eba6a` — the two commits between them (#460, #461)
touch the Win32 rail and its tests only, and leave the scenario registry digest
unchanged.

## Contents

**All 16 registered cells**, each with its `.profile`, plus `SHA256SUMS`.

scrapbook 6 / helloworld 3 / minesweeper 3 / tutorial 2 / floppybird 2

## The three that were missing, and why

```
scrapbook open-first-page-refused
scrapbook refused-flip-keeps-page
scrapbook open-text-page-refused
```

**#462, closed by #467.** `tests/macos/run-scenario.sh` never staged the
corrupt-package fixture that `tests/scenarios/scrapbook-package-fixtures.txt`
declares for these three cells. Win32 did it and Classic did it; the macOS
runner had no reference to the registry, to `lrpc`, or to the asset at all. The
package loaded intact, the scenario asked for a refusal that never came, and the
audit correctly reported that its expectations were not met.

These cells were not "not yet baked" — they could not pass on this rail, and
#407 had already recorded the same hole five days earlier. #467 moved the
staging policy into one guard every rail calls, and gave this rail a staged copy
of its own bundle to corrupt. Verified on this rig: red at `origin/main`
(`differ: char 72, line 2`, the offset #462 recorded), green after.

The whole registry then passed the pixel verdict twice, 16/16.

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
