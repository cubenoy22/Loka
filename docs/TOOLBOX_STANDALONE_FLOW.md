# Toolbox 68K standalone Flow stage

## Autonomous release

Run:

```sh
./scripts/toolbox-standalone-flow.sh Release
```

The release under `build/release/toolbox-68k` contains five autonomous
Standalone Loop applications plus the interactive `LokaSimpleViewer68K`.
Each application is provided as both a MacBinary (`.bin`) and an 800 KiB HFS
disk (`.dsk`). The Scrapbook loop disk also contains `ASSETS.LRP`; the same
asset file is included separately for MacBinary and SCSI-disk workflows.

The loop applications keep one App and native Window alive, replace their
completed scenario rail, and re-arm the current Scene until the user closes
the Window. SimpleViewer remains interactive because its file chooser requires
user selection.

## Finite Scrapbook presentation stage

This directory is a transportable Retro68 release of the Scrapbook standalone
presentation Flow. It contains:

- `LokaScrapbookStandaloneFlow68K.bin`: the MacBinary application for copying
  to an existing HFS volume;
- `LokaScrapbookStandaloneFlow68K.dsk`: a self-contained 800 KiB HFS floppy
  image containing the application and `ASSETS.LRP`; and
- `ASSETS.LRP`: the application data file, provided separately for MacBinary
  and SCSI-disk workflows.

## Real Classic Mac hardware

Use a binary-safe transfer method for the `.bin` file, decode it on the target
HFS volume, and put `ASSETS.LRP` beside the resulting application. As an
alternative, write or mount the `.dsk` with a tool that preserves a raw HFS
floppy image; that image already contains both required files.

## MAME floppy

Configure `.env-mame` as described in `docs/MAME_DEVELOPMENT.md`, start MAME,
then run the VS Code task **Stage & Mount in Running MAME: Scrapbook Standalone
Flow**. The equivalent manual mount command from the repository root is:

```sh
./scripts/mame-mount.sh \
  build/presentation/toolbox-68k-release/LokaScrapbookStandaloneFlow68K.dsk
```

Open `LokaScrapbookStandaloneFlow68K` from the inserted floppy.

## MAME SCSI

Stop MAME before rebuilding its development disk, then run **Stage & Start in
MAME via SCSI: Scrapbook Standalone Flow**. The task derives the local
`build/mame-dev/LokaDev.hd` from the boot-disk template configured by
`MAME_HDA`, copies this stage's `.bin` and `ASSETS.LRP` to it, and starts MAME
with that disk at SCSI ID 5.

The stage intentionally does not contain a boot or SCSI hard-disk image: that
image depends on the local licensed Classic Mac OS installation and is safely
generated without modifying the configured template.
