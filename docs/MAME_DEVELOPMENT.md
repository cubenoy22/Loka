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

## Source-level debugging with gdb (68K only)


A Retro68 application running from the SCSI development disk can be debugged
at source level: breakpoints by function or line, argument values, and a
multi-frame backtrace. This was demonstrated under MAME on a 68030 machine
(e.g. `maciix`) with the Tutorial example, stopping in `App::idlePolicy` with `App::consumeIdle` and
`ToolboxApp::run` resolved on the stack.

Seven things all have to be right. Missing any one of them looks like the
approach does not work at all, which is the trap this section exists to
close.

### Host requirements

This workflow needs a gdb that understands m68k. **Retro68 does not ship
one.** On Linux and WSL the distribution's `gdb-multiarch` works and is what
this was developed and verified against. macOS has no equivalent package, and
Retro68 builds there run inside a Linux container while MAME runs on the host,
so the debugger would have to be built or placed deliberately; that
combination is untested.

Linux/WSL is therefore the practical host: the Retro68 toolchain, the build
output, and the debugger all sit in the same environment, and only MAME is
reached across the boundary.

### 1. Build with debug information

The ordinary Retro68 release profile has no `-g`, so the emitted
`*.code.bin.gdb` carries symbols but no line table. Configure a separate
build directory with DWARF:

```sh
cmake --preset retro68-68k-dwarf
cmake --build --preset retro68-68k-dwarf --target LokaTutorial68K_APPL
```

The result is an `MC68000 ELF32` file with DWARF 4 whose `DW_AT_name` entries
point at the real source paths.

### 2. Find the runtime load base

The Segment Loader decides where the CODE resources land, so symbols have to
be relocated to that address. Search memory for a function whose bytes cannot
have been altered by startup fixups — one with no relocation entries across
its range:

```sh
ELF=build/retro68/68k/DiagDwarf4/example/Tutorial/LokaTutorial68K.code.bin.gdb
SECTION=.code00002   # the executable section holding application code

# bounds of that section
read -r SEC_ADDR SEC_SIZE < <(readelf -SW "$ELF" |
  awk -v s="$SECTION" '$2==s {print strtonum("0x"$4), strtonum("0x"$6); exit}')

# every relocation offset that applies to it
readelf -rW "$ELF" | awk -v s="rela$SECTION" '
  /^Relocation section/ {inseg = index($0, s) > 0; next}
  inseg && $1 ~ /^[0-9a-f]{8}$/ {print strtonum("0x"$1)}' | sort -n > /tmp/relocs

# functions inside the section with no relocation anywhere in their range
readelf -sW "$ELF" | awk '$4=="FUNC" && $3+0>=64 {print strtonum("0x"$2), $3, $8}' |
  awk -v lo="$SEC_ADDR" -v hi="$((SEC_ADDR+SEC_SIZE))" '
    NR==FNR {r[++n]=$1; next}
    { a=$1+0; sz=$2+0
      if (a<lo || a>=hi) next
      for (i=1; i<=n; i++) if (r[i]>=a && r[i]<a+sz) next
      printf "0x%08x %6d %s\n", a, sz, $3 }
  ' /tmp/relocs - | sort -k2 -nr | head -5
```

Take the longest candidate; a longer function makes a false match vanishingly
unlikely. For the Tutorial this reports
`ToolboxRectSurfaceContext::dirtyRect` at `0x00029372`, chosen from 48
relocation-free functions out of 645 candidates and 3224 relocations.

Take the first eight bytes of a clean function and pass them, with its link
address, to `scripts/mame-find-base.lua` as the autoboot script:

```sh
LOKA_PATTERN_WORD0=4feffefc LOKA_PATTERN_WORD1=48e71e30 \
LOKA_PATTERN_LINK=00029372 LOKA_STAY_ALIVE=1 \
  ...  -autoboot_script scripts/mame-find-base.lua
```

It bounds the search with `ApplZone` and `ApplLimit`, so it covers the
application heap rather than all of RAM — a few hundred KB, and a match in
seconds. The base was reproducible across independent runs for a given
binary, but it moves when the binary changes, so read it rather than
hardcoding it.

### 3. Relocate every section with one offset

Retro68 links the image contiguously from vaddr 0 (`.code00001` at `0x0`,
`.code00002` at `0xbff4`, then `.data` and `.bss`), so a single offset is
enough. There is no need to place sections individually:

```
add-symbol-file <elf> -o <base>
```

### 4. Use hardware breakpoints for the first stop

MAME's gdbstub halts the machine at reset, long before the application is
loaded. A software breakpoint planted then is overwritten when the CODE
resources are read in. Use `hbreak` to reach the first stop.

Once the application is loaded, ordinary software breakpoints work normally,
so anything set after that first stop behaves as usual.

### 5. One gdb session per MAME run

The stub does not accept a second connection; a reconnect fails with
`Connection reset by peer`. Even a bare TCP probe consumes the slot, so do not
test reachability before attaching — connect with gdb directly.

### 6. Reach the stub by host address, not loopback

MAME listens on the Windows loopback interface. Under WSL2 that is a separate
network namespace, so `localhost:23946` never connects. Use the host address:

```sh
gdb-multiarch -ex "set architecture m68k" -ex "set endian big" \
  -ex "target remote $(ip route | awk '/default/{print $3}'):23946"
```

### 7. Forward the script's variables into the Windows process

`scripts/mame-find-base.lua` is configured through environment variables. WSL
does not pass its environment to a Windows executable unless the names are
listed in `WSLENV`, so under WSL the launcher must export it as well or the
script aborts on the first `required()` call:

```sh
export LOKA_PATTERN_WORD0=4feffefc LOKA_PATTERN_WORD1=48e71e30
export LOKA_PATTERN_LINK=00029372 LOKA_GDB_LOG='C:\path\to\find-base.log'
export LOKA_STAY_ALIVE=1   # only when attaching gdb in the same run
export WSLENV=LOKA_PATTERN_WORD0:LOKA_PATTERN_WORD1:LOKA_PATTERN_LINK:LOKA_GDB_LOG:LOKA_STAY_ALIVE
```

Paths handed to a Windows MAME stay in Windows form; do not add the `/p`
translation flag to them.

### Why this does not carry over to PPC

Two properties of the 68K output make the method above work, and the PowerPC
path has neither.

The 68K toolchain runs `Elf2Mac`, producing CODE resources from an image
linked contiguously from vaddr 0, so a single `-o` offset relocates every
section. The PowerPC toolchain runs `MakePEF`, producing a PEF container
loaded by the Code Fragment Manager: code and data are separate fragments
with their own loader relocations, a TOC register, and transition vectors.
One offset cannot express that mapping.

The base is recovered here by finding bytes that startup cannot have
rewritten. CFM applies its own relocations when it prepares a fragment, so
"identical in the file and in memory" is not a property one can rely on the
same way.

Nothing available understands PEF well enough to close the gap: gdb handles
the PowerPC instruction set but not the CFM container.

Treat MAME plus 68K as the rig for looking **inside** a running application,
and PowerPC as a leg for confirming that an application **runs**. Those are
different jobs; expecting the second to provide the first will disappoint.

### Known limits

- `-Os` leaves no frame pointer, so unwinding past the platform entry point
  produces a bogus outermost frame. The application frames are correct.
- A5-relative globals are not covered by this; only code addresses are
  relocated by the offset above.
- **A wild pointer write does not necessarily bomb.** Writing through an
  unmapped address (`0xDEADBEEE` was tried) on a 68030 machine neither faults nor
  logs; the application keeps running as if nothing happened. The 68030 also
  permits misaligned word and long accesses, so the address errors a 68000
  would raise are not available either. Memory-lifetime defects on Classic can
  therefore corrupt silently rather than announce themselves, which is an
  argument for watchpoints (`wpset`) over waiting for a crash to locate them.

## Verification status

- macOS Tahoe: runtime-verified on Intel and Apple silicon (A18 Pro) with MAME
  0.288 on `maciici`, including live floppy insertion and the generated
  `LokaDev` SCSI volume.
- Windows through WSL: runtime-verified with MAME 0.287 on `maciix`, including
  the combined Retro68 build, generated `LokaDev` SCSI disk, and host-side MAME
  startup from a WSL-hosted VS Code window.
- Source-level gdb debugging: runtime-verified on Windows on ARM through WSL,
  with a native Aarch64 MAME 0.287 on a 68030 machine. The emulated machine ran at
  roughly 6.7x real time, so a full boot-and-launch cycle takes about two
  minutes.
