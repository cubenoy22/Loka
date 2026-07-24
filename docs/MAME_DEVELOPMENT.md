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

## Automated runtime verification

The launch scripts above provide the interactive development workflow. A
bug-specific automated scenario can wrap the same build artifacts with a MAME
Lua autoboot script. Keep scenario launchers, copied disks, logs, and snapshots
under a purpose-named directory such as `build/runtime-103/`; they are test
artifacts, not application sources.

### Isolate the emulator state

Automated runs must not use the installed boot disk as a writable runtime
disk. Stop MAME, copy the configured boot HDA into the scenario directory, and
point `-hard1` at the copy. Generate a scenario-local `LokaDev.hd` with
`scripts/mame-dev-disk.sh` or `scripts/mame-dev-disk.ps1`, and attach it as
`hard2` at SCSI ID 5. The source HDA and MAME installation then remain inputs,
while all mutable state stays under `build/`.

Give each scenario its own MAME directories as well:

```text
-homepath           build/runtime-<id>/home
-cfg_directory      build/runtime-<id>/home/cfg
-nvram_directory    build/runtime-<id>/home/nvram
-snapshot_directory build/runtime-<id>/home/snap
-diff_directory     build/runtime-<id>/home/diff
```

Rebuild the Retro68 `_APPL` target before preparing `LokaDev.hd`. Stop MAME
before replacing either disk; a disk mounted by the emulator is not safe to
rewrite.

### Headless by default, visible when reviewing

Use the same Lua scenario for unattended verification and a reviewer-visible
replay. Only the MAME presentation and throttle policy should differ:

| Mode | MAME options | Intended use |
| --- | --- | --- |
| Headless | `-video none -sound none -nothrottle` | Routine verification without taking host focus |
| Visible | `-window -nomaximize -sound none -nothrottle` | Temporary human review of the exact automated sequence |

Booting Classic Mac OS dominates the run time. Let the boot and Finder setup
run unthrottled, then set `manager.machine.video.throttled = true` in Lua just
before the behavior that a reviewer needs to see. This keeps startup short
without making the relevant interaction flash past. A visible run may take
host focus, so do not use it as the default from WSL or an automated agent.

### Drive the emulated machine from Lua

Pass a scenario with `-autoboot_script` and use emulated time (`emu.wait`) for
sequencing. Host sleeps are less reliable because `-nothrottle` changes the
relationship between host and emulated time.

Useful MAME Lua surfaces are:

- `manager.machine.natkeyboard` for ordinary text and coded keys;
- `manager.machine.ioport.ports` for ADB modifiers, mouse axes, and buttons;
- `screen:snapshot(path)` for evidence that does not require a host screenshot;
- `manager.machine:exit()` for deterministic completion.

I/O port tags and field names are machine-specific. Enumerate them once for the
selected machine instead of assuming that a host key or mouse provider maps to
the Classic Mac input. On `maciix`, the ADB keyboard and mouse are exposed
under `:macadb:KEY*` and `:macadb:MOUSE*`. Direct field control is especially
useful for Command-key combinations that the natural-keyboard coded-text API
does not express.

Finder name selection plus an emulated `Command+O` is more repeatable than
host-coordinate mouse automation for opening `LokaDev` and its application.
Once the app is running, prefer input through the real Toolbox event path. If
an exact platform failure cannot be discriminated through MAME input alone, a
small application-side event probe may be used in a temporary verification
build. Such a probe is evidence, not production code: remove it immediately,
rebuild the ordinary `_APPL` target, and regenerate the development disk after
capturing the result.

### Record the verification claim

Runtime evidence should name the machine, the input sequence, and the observed
result. Preserve the final snapshot and relevant console output under the
scenario directory. Distinguish the claims explicitly:

- **build-verified** means the actual Toolbox translation unit compiled and
  linked for the Retro68 target;
- **runtime-verified** means MAME booted the generated artifact and the
  automated sequence exercised the claimed behavior.

For a bug fix, the runtime sequence should mirror the failure mode rather than
merely prove that the example launched. A useful scenario states which control
was focused, which structural change occurred, and which visible or state
result proved that the surviving control remained functional.

## Verification status

- macOS Tahoe: runtime-verified on Intel and Apple silicon (A18 Pro) with MAME
  0.288 on `maciici`, including live floppy insertion and the generated
  `LokaDev` SCSI volume.
- Windows through WSL: runtime-verified with MAME 0.287 on `maciix`, including
  the combined Retro68 build, generated `LokaDev` SCSI disk, and host-side MAME
  startup from a WSL-hosted VS Code window.
