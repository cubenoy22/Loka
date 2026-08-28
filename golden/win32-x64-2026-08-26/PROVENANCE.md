# Win32 (x64) goldens — 2026-08-26 generation

The 0.0.4 set. Baked on the reference Win32 rig at `main = a36148fa`, rig id `omen`.

Two cells: `scrapbook startup` (L1) and `scrapbook flip-forward-back` (L2), the
cells the release matrix names for this rail. Both files hash identically
(`7e782f2b…`), which is the identity `tests/scenarios/startup-golden-identities.txt`
declares: `flip-forward-back` flips forward and back, so it settles on page 1 —
the same picture `startup` shows.

## What this set is, and is not

It records **what this version drew in this environment**. It is not a claim that
the pixels are correct, and — see below — it is not usable as a long-term baseline.

Known drawing defects present in these captures:

- **#481** — the `*` overlapping `Loka Sample` in HelloWorld is the example's
  deliberate ZStack overlay demonstration, not a drawing defect (clarified by
  the maintainer 2026-08-28); it is a legibility question, not correctness.
- **#45 item 7** — wrap-mode measurement diverges across rails, silently truncating
  a label on Win32 and macOS. Not in these two cells; recorded because it is in
  this build and shows on other Win32 cells.

## Display environment — recorded by hand, because the profile cannot

**The `.profile` files beside these goldens say `scale_percent=100`. That value is
wrong, and it is wrong on every Win32 capture this rail has ever taken (#492).**
Loka declares no DPI awareness, so `GetDpiForWindow` always answers 96 and the
profile records a constant rather than a fact about the machine.

Measured from a per-monitor-aware probe at bake time:

```
GetDpiForSystem()                  192  => 200%
GetDeviceCaps(screenDC, LOGPIXELSX) 192
screen metrics                     CXSCREEN=2880 CYSCREEN=1800
monitors                           1: (0,0)-(2880,1800) effectiveDPI=192 => 200% [PRIMARY]
```

The previous Win32 generation (`win32-x64-2026-08-22/`) was baked on a different
desk: two monitors, 3840x2160 at 150% plus one at 200%, `GetDpiForSystem() = 144`.
Comparing this build against that generation's golden produced **29,076 differing
pixels across 300 columns** — the image view stretches its 200x150 source to the
view rect, so a different effective scale changes the nearest-neighbour ratio and
the dither's mesh with it. The profile guard passed that comparison, because all
seven declared fields matched. That is #492.

## Not a long-term baseline

Per the golden-lifetime ruling, a long-term golden is valid only where the OS
environment of the snapshot can be reproduced later. This rail is bare metal: the
desk moves, Windows updates, and the display configuration is not recorded by
anything the rail reads. **Treat this set as release evidence and as the base for
an explained version-to-version comparison, not as a baseline a mismatch should be
measured against.** The Classic (MAME) sets are the ones that carry a reproducible
environment.

## Bake note

`scrapbook startup` was baked five times before this set was kept. The first bake
captured a minority settle value (`f25142…`); four subsequent bakes all landed on
the majority (`5dca74…` content, `7e782f2b…` cropped), and the startup-identity
guard is what caught the first one by refusing `flip-forward-back` against it.
See #493. The kept set is the majority picture.

## Why this lives here and not in the tracked tree

Rig-local goldens are untracked: they depend on the machine's fonts and chrome.
Reviewers see the captures through this evidence branch instead.
