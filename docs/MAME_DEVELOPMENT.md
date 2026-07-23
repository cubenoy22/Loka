# MAME development workflow

The VS Code tasks provide two Retro68 application delivery paths for MAME:

- floppy tasks keep MAME running and insert a generated `.dsk` on demand;
- SCSI tasks rebuild a development hard disk and start MAME with that disk at
  SCSI ID 5.

The SCSI path is the faster default for application iteration. The floppy path
remains available for live disk insertion without restarting MAME.

## Quick start

1. Install MAME and set up the ROMs and a bootable HDA for a supported Classic
   Mac machine such as `maciici`.
2. Copy `.env-mame.example` to `.env-mame` and set `MAME_HDA`. Also set
   `MAME_EXECUTABLE` and `MAME_ROMPATH` when MAME cannot find them
   automatically.
3. In VS Code, run **Tasks: Run Task** and select
   `Build & Start in MAME via SCSI: HelloWorld` (or another example).

The task configures and builds the Retro68 application, creates the generated
`LokaDev` SCSI disk, and starts MAME. For the live floppy workflow, start
`MAME: Start` first and then run an app-specific `Build & Mount in MAME` task.
When VS Code is connected to WSL, the same tasks prepare the disk with the WSL
Retro68 tools and delegate MAME startup to the Windows PowerShell launcher.
Use Windows paths for the MAME executable, ROMs, and boot HDA in `.env-mame`.

## Configuration

Copy `.env-mame.example` to `.env-mame` and set at least `MAME_HDA`. The file
is local and ignored by Git. `MAME_HDA` is both the boot disk and the template
used to create `build/mame-dev/LokaDev.hd`; the original image is never
modified by the development-disk script.

Set `RETRO68_BUILD_DIR` in the environment when the Retro68 build is not under
`~/Retro68-build` or `~/Retro68`. `RETRO68_TOOLCHAIN_BIN` may also be set in
`.env-mame` when the host `hformat`, `hcopy`, and `humount` tools are not on
`PATH` or in one of the standard Retro68 build locations.

## SCSI workflow

Run `Build & Start in MAME via SCSI: <App>`. The task:

1. builds the app's Retro68 `_APPL` target;
2. copies the configured boot HDA to a temporary generated image;
3. preserves its Apple partition map and SCSI driver partition;
4. reformats only the first HFS partition as `LokaDev`;
5. copies the Retro68 MacBinary app into that partition; and
6. starts MAME with the boot disk as `hard1` and `LokaDev` as `hard2`.

Stop MAME before rebuilding the SCSI development disk. A fixed SCSI disk is
not safe to rewrite while the emulator has it open.

## Floppy workflow

Run `MAME: Start` once, then use `MAME: Mount .dsk (pick app)` or one of the
app-specific mount tasks. A small MAME Lua service accepts mount and eject
requests through files under `MAME_CONTROL_DIR`. The combined `Build & Mount`
tasks eject before rebuilding so Retro68 never rewrites a mounted `.dsk`.

## Verification status

- macOS Tahoe: runtime-verified on Intel and Apple silicon (A18 Pro) with MAME
  0.288 on `maciici`, including live floppy insertion and the generated
  `LokaDev` SCSI volume.
- Windows through WSL: runtime-verified with MAME 0.287 on `maciix`, including
  the combined Retro68 build, generated `LokaDev` SCSI disk, and host-side MAME
  startup from a WSL-hosted VS Code window.
