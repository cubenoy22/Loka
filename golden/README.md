# Classic rail golden archive

Durable copies of the Classic (MAME) scenario pixel goldens.

## Why they live here and not in the tracked tree

The Classic goldens are produced by booting a real System disk image under MAME,
so every capture contains Apple-rendered font glyphs and control chrome from that
boot image. `AGENTS.md` (Licensing) keeps such content out of the tracked tree,
and `tests/toolbox/run-scenario.sh` records the same reason at the point where
the golden path is defined, adding that reviewers see these captures through the
`pr-assets` evidence branch instead. This directory is that branch's home for
complete golden sets, as opposed to the per-PR capture directories beside it.

`pr-assets` is an evidence branch. Nothing here is part of a release artifact and
nothing here is merged into `main`.

## Why an archive is needed at all

The rail keeps its working goldens under `build/mame-scenario/golden/`, which is
git-ignored. A routine `build/` wipe destroys the baseline, and by 2026-08-18
three divergent sets already existed across different clones with no way to tell
their generations apart, because the only provenance recorded beside each PNG was
a `.mame-machine` sidecar that says `maciix` for all of them.

## Layout

One directory per generation, named `classic-<mame-machine>-<capture-date>`.
Each holds the scenario tree as the rail expects it, a `SHA256SUMS` manifest, and
a `PROVENANCE.md` describing what produced it.

## Restoring a generation

```sh
cd <generation> && sha256sum -c SHA256SUMS
cp -r <generation>/. <loka>/build/mame-scenario/golden/
```

Read that generation's `PROVENANCE.md` first: a golden is only a baseline for the
rig that produced it, and the boot image identity is the part currently least
well recorded.
