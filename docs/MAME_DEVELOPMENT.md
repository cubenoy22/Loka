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
5. copies the Retro68 MacBinary app and any requested plain data files into
   that partition; and
6. starts MAME with the boot disk as `hard1` and `LokaDev` as `hard2`.

Stop MAME before rebuilding the SCSI development disk. A fixed SCSI disk is
not safe to rewrite while the emulator has it open.

The development-disk scripts keep their original one-argument form and accept
additional plain data files after the app. For example, ScrapbookUI ships its
LRPK package beside the application:

```sh
./scripts/mame-dev-disk.sh \
  build/retro68/68k/Release/example/ScrapbookUI/ScrapbookUI68K.bin \
  example/ScrapbookUI/ASSETS.LRP
```

The PowerShell twin accepts the same positional arguments.

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

Three Lua behaviors cost real time to rediscover:

- Device enumerators such as `manager.machine.screens` are not plain tables;
  generic iteration with `next()` fails on them. Index by tag
  (`manager.machine.screens[":screen"]`) instead.
- An error thrown inside the autoboot script does not stop MAME. A headless
  run then sits forever with nothing on the console. Wrap the scenario body
  in `pcall` and call `manager.machine:exit()` on both the success and the
  failure path, so a broken scenario terminates and reports instead of
  hanging the harness.
- Relative mouse deltas pass through the Mac's pointer-tracking curve, which
  is non-linear, so a computed delta does not land the pointer at a
  predictable position. Do not use the `:macadb:MOUSE1`/`MOUSE2` axes for
  positioning at all; warp the cursor through the low-memory globals instead
  (next section). The axes are also the only pointer inputs available:
  `maciix` has no ADB slot (`-listslots`), so no absolute pointing device can
  be attached, and an ioport dump shows no lightgun or positional field
  anywhere on the machine.

Finder name selection plus an emulated `Command+O` is more repeatable than
host-coordinate mouse automation for opening `LokaDev` and its application.
Once the app is running, prefer input through the real Toolbox event path. If
an exact platform failure cannot be discriminated through MAME input alone, a
small application-side event probe may be used in a temporary verification
build. Such a probe is evidence, not production code: remove it immediately,
rebuild the ordinary `_APPL` target, and regenerate the development disk after
capturing the result.

### Click at absolute coordinates: warp the cursor through low-memory globals

Positioning does not need the ADB mouse at all. Classic Mac OS keeps the
pointer state in low-memory globals, and the cursor task re-reads them at VBL
when told to; writing them from Lua warps the pointer to an exact coordinate:

```lua
local mem = manager.machine.devices[":maincpu"].spaces["program"]
local couple = mem:read_u8(0x8CF)          -- CrsrCouple
local function warp(h, v)
    mem:write_u16(0x828, v)                -- MTemp.v
    mem:write_u16(0x82A, h)                -- MTemp.h
    mem:write_u16(0x82C, v)                -- RawMouse.v
    mem:write_u16(0x82E, h)                -- RawMouse.h
    mem:write_u8(0x8CE, couple ~= 0 and couple or 1)  -- CrsrNew := CrsrCouple
    emu.wait(1)                            -- cursor task picks it up at VBL
end

local btn = manager.machine.ioport.ports[":macadb:MOUSE0"].fields["Mouse Button 0"]
local function clickAt(h, v)
    warp(h, v)
    btn:set_value(1)
    emu.wait(0.3)
    btn:clear_value()
    emu.wait(0.3)
end
```

**Never warp the cursor away in the same instant as the release.** Classic
push buttons track the mouse and fire only if the up lands inside the
control, and the warp takes effect within a VBL tick — so
`btn:clear_value(); warp(320, 20)` with no wait between them displaces the
release off the button and silently cancels the click. The press highlight
still shows, which makes the resulting non-action read as an application bug:
issue #324 was filed, investigated on two fronts, and closed over exactly
this. The `clickAt` above already pauses after release; keep that shape, and
if a scenario must park the cursor somewhere neutral afterwards, wait at
least 0.2 emulated seconds after `clear_value()` before warping. (MineSweeper's
New Game happened to fire even with a release-instant warp — see the TODO on
button release semantics — so a scenario that "works" in one app is not
evidence the shape is safe in another.)

The button still goes through the ADB ioport field, which is reliable; only
the axes were ever the problem. Verified on `maciix` under KanjiTalk 7
(#182): `warp(200, 150)` puts the arrow at exactly that pixel in the snapshot,
and `warp(16, 8)` plus a held button opens the Apple menu — the click lands
where the warp pointed. Points are stored v-first (`RawMouse` at `$82C`,
`MTemp` at `$828`); allow about one emulated second after the warp before
pressing the button. The addresses are universal 68K low-memory globals, but
other machines and System versions are unverified. Applications observe the
warped position through the normal `GetMouse`/event path, so this is the tool
for reaching a control that keyboard navigation cannot.

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

### 4. Wait for the listener before attaching, without probing it

MAME holds the machine at reset until a debugger connects, so the autoboot
script does not begin until gdb continues. Attaching too early instead fails
with `Connection reset by peer`, and the window is narrow enough that
launching MAME and gdb back to back is not reliable.

Wait for the listener with a tool that reads socket state rather than opening
a connection. `netstat` does; a TCP probe does not, and would consume the
stub's single slot (see the next point):

```sh
until netstat.exe -an | grep -q "23946.*LISTENING"; do sleep 1; done
sleep 4
```

Ordinary breakpoints are what to use once attached. `hbreak` is rejected --
`No hardware breakpoint support in the target` -- and is not needed: MAME
implements a software breakpoint as an emulator-side check rather than by
writing a trap instruction into memory, so one planted before the application
is loaded survives its CODE resources being read in over that address.

Place breakpoints by symbol, not by file:line. Support-library objects link
DWARF 5 compilation units in beside the application's DWARF 4 ones, and
resolving a file:line location walks the mixed line tables and killed
gdb-multiarch in practice. Symbol breakpoints (`break App::idlePolicy`) do
not consult the line table and are unaffected; `list` and source display
still work once stopped.

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

### Worked example, start to finish

`scripts/mame-debug.sh` assembles the launch: it stages a scenario-local copy
of the boot disk, generates the development disk, forwards the variables the
Lua needs, waits for the listener, and hands over to gdb with symbols already
relocated. Everything mutable lands under `build/mame-debug/`, so the
configured boot disk stays an input.

**1. Build with debug information**

```sh
cmake --preset retro68-68k-dwarf
cmake --build --preset retro68-68k-dwarf --target LokaTutorial68K_APPL
APPL=build/retro68/68k/DiagDwarf4/example/Tutorial/LokaTutorial68K.bin
```

**2. Find the load base**

Pick a relocation-free function with the recipe above and pass its first eight
bytes and link address. This boots the application once and exits:

```sh
./scripts/mame-debug.sh find "$APPL" 4feffefc 48e71e30 00029372
# LOKA-BASE: BASE 0x0070e2a4
```

Re-run this whenever the application binary changes; the base moves with it.

**3. Attach**

Same arguments plus the base. The machine is left running and gdb takes over:

```sh
./scripts/mame-debug.sh attach "$APPL" 4feffefc 48e71e30 00029372 0x0070e2a4
```

Symbols are loaded at the offset and a breakpoint is set on `App::idlePolicy`,
which runs continuously once the application is up and so makes a convenient
first stop. Set `LOKA_BREAK` to choose a different one. Then `continue`: after
the first stop, further breakpoints, `bt`, `info args`, and `list` behave as
usual.

**4. Batch sessions without a human at the prompt**

Two optional environment variables extend the launcher for scripted use:

- `LOKA_DEV_DATA` — newline-separated plain data files (one path per line)
  copied onto the development disk beside the application. Each path is passed
  as its own trailing argument to `mame-dev-disk.sh`. A data-driven application
  such as ScrapbookUI refuses at startup without its `ASSETS.LRP`, before ever
  reaching the code under debug.
- `LOKA_GDB_SCRIPT` — a second gdb command file executed after the attach
  script, so an unattended session can plant its own breakpoints, log what it
  needs, and quit.

When gdb exits — from a scripted quit or an interactive one — the launcher
terminates the MAME it started. The stub refuses reconnects, so a leftover
emulator could only squat on the debug port and break the next run.

```sh
LOKA_DEV_DATA=example/ScrapbookUI/ASSETS.LRP \
LOKA_GDB_SCRIPT=build/mame-debug/trace.gdb \
  ./scripts/mame-debug.sh attach "$APPL" 4feffefc 48e71e30 00029372 0x0070e2a4
```

Two phases rather than one because gdb needs the base before it can place a
breakpoint by symbol, and the base is only discoverable once the application
has been loaded — which cannot happen while gdb is holding the machine at
reset waiting to be told to continue.

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
  relocated by the offset above. Reading such a global through gdb therefore
  returns the wrong memory. Function arguments and locals are frame-relative
  and read correctly, so when a global's value has to be observed, break in a
  function that receives it (or a value derived from it) as an argument.
- **The stub does not relay guest CPU exceptions.** Probed on #182 with a
  planted `__builtin_trap()` (ILLEGAL, vector 4): without gdb the same build
  bombs on screen (System Error type 11) at exactly that point, while an
  attached session with no breakpoint armed sees no stop at all — the 68k
  vectors into the system error handler and keeps running, which to the stub
  is ordinary guest control flow. A Classic bomb therefore cannot be caught
  at the faulting instruction through the stub. If exception catching is ever
  needed, read the exception vectors at runtime (`0x08` bus error, `0x0C`
  address error, `0x10` illegal instruction) and plant an ordinary breakpoint
  on the handler.
- **Avoid `delete` in scripted stub sessions.** Across the #182 probe runs,
  "Cannot execute this command while the target is running" appeared
  deterministically whenever the batch script executed `delete` — refused
  outright after a breakpoint stop, or poisoning the next `continue` when
  issued at attach time — and never blocked a session without it. To replace
  the attach-time breakpoint, override it with `LOKA_BREAK` instead of
  deleting it; after a stop, observation commands (`info registers`, `x`,
  `bt`) work. `run-wpset.sh` does use `delete` and has run green, so the
  trigger may be probabilistic — treat `delete` as hazardous, not banned.
### The watchpoint leg: freed-memory checks for real teardown

`tests/toolbox/run-wpset.sh` turns the wpset idea above into an automated
scenario leg. It derives the pattern words with the readelf recipe, runs the
find phase (caching the base per ELF content), and attaches with a batch gdb
script that stops at the *teardown* `ScrapbookPackage::close` — a breakpoint
condition (`this->open_ && this->currentBag_ >= 0`) skips the defensive
close inside `open()`, which otherwise makes the whole leg pass vacuously
with nothing armed. At the stop it reads the ui-bag, current-bag, and index
buffer addresses through `this` (DWARF member navigation works at symbol
breakpoints), arms full-range access watchpoints on all three regions
*before* the close body executes — the releases happen inside `close`, so
arming afterwards would leave the rest of the function unobserved (close
touches no payload bytes itself, so the live-at-entry watches do not
false-positive) — and then continues to the driver's noinline
`ScenarioTeardownComplete()` beacon, which is reached only after the App
and PlatformContext are destroyed. Any touch of freed bag memory anywhere
in that window fires in the log, and the verdict requires both the armed
watchpoint listing and the beacon's breakpoint hit, so neither a vacuous
arm nor an early exit can pass silently. The `control`
mode proves the mechanism can fire at all: a read watchpoint on a *live*
committed bag buffer must trip on the next redraw. The MAME gdbstub
implements gdb's Z2/Z3/Z4 packets as hardware watchpoints (wpset), so
neither mode pays the single-step penalty.

- **A wild pointer write does not necessarily bomb.** Writing through an
  unmapped address (`0xDEADBEEE` was tried) on a 68030 machine neither faults nor
  logs; the application keeps running as if nothing happened. The 68030 also
  permits misaligned word and long accesses, so the address errors a 68000
  would raise are not available either. Memory-lifetime defects on Classic can
  therefore corrupt silently rather than announce themselves, which is an
  argument for watchpoints (`wpset`) over waiting for a crash to locate them.

### The sampling leg: attributing a busy window

The stub also supports a time-sampling profiler: interrupt the free-running
guest on a wall-clock cadence, read the interrupted PC, resume, repeat. No
instrumentation goes into the build, so the workload under measurement is the
shipping code. This is what attributed the first-launch cost on a 68030
machine to the ROM Memory Manager zone-scan loop (14 of 14 samples, with
`LokaAllocRaw`/`_NewPtr` frames on the raw stack) — an answer neither
breakpoints nor watchpoints could have produced, because nothing was known to
break on.

Two session shapes work; pick by whether the window of interest starts at a
place a breakpoint can name.

**Batch shape** — when the window opens at a known function (e.g. sampling
startup from `main`): a plain `gdb -x` script alternating
`continue &` / `shell sleep 0.5` / `interrupt` / `bt 16`. Simple, but the
pacing is fixed at script-writing time and the session cannot react to the
guest.

**FIFO driver shape** — when the window opens at a scenario event and the run
must stay unperturbed until then. The driver creates a FIFO, starts
`gdb-multiarch` reading commands from it with output to a log file, pumps
commands with `printf`, and synchronizes by grepping the log for `echo`
markers it planted:

```sh
mkfifo "$FIFO"                       # must live on ext4 — mkfifo fails on DrvFs (/mnt/c)
gdb-multiarch -q >"$OUT" 2>&1 <"$FIFO" &
GDB_PID=$!; exec 3>"$FIFO"
send() { printf '%s\n' "$1" >&3; }

# Breakpoint-free attach. scripts/mame-attach.gdb is not reusable here: it
# arms a breakpoint at attach time, and this leg requires none armed.
send "set confirm off"
send "set pagination off"
send "set height 0"
send "set architecture m68k"
send "set endian big"
send "set remotetimeout 120"
send "target remote 192.168.0.1:23946"   # host address, not loopback (step 6)
send "add-symbol-file $ELF -o $BASE"     # base from the find phase (step 2)
send "continue"                           # free-run toward the scenario event

until [ -f "$RT/click-1.flag" ]; do sleep 1; done   # marker file, not a breakpoint
kill -INT "$GDB_PID"                 # first stop: the window of interest is open
sleep 0.7

sample() {                           # <label> <run-seconds>
  send "echo \\n--- $1 ---\\n"
  send "continue"
  sleep "$2"
  kill -INT "$GDB_PID"               # SIGINT to gdb stops the guest through the stub
  sleep 0.7
  send "x/1wx \$sp+2"                # the interrupted PC (see below)
  send "x/64wx \$sp"                 # raw stack for offline reading
  send "echo \\n--- $1 end ---\\n"
  # ...grep "$OUT" for the end marker before the next sample
}
```

Hard-won mechanics, each paid for in lost sessions:

- **The interrupted PC is the longword at `$sp+2`.** A SIGINT stop lands in
  the 68k interrupt exception frame (status register word at `$sp`, pushed PC
  at `$sp+2`), and `bt` at such a stop frequently unwinds the interrupt
  context rather than the interrupted code. Read `$sp+2` for the PC and dump
  `x/64wx $sp` raw; pick return-address candidates out of the words offline.
- **Resolve addresses offline, not in the session.** Subtract the runtime
  load base and feed `addr2line -f -C` against the DWARF ELF; classify
  `>= 0x40000000` as ROM and anything outside `[base, base+image)` as
  other-RAM. The base changes with every build — re-run the find phase from
  step 2 whenever the ELF changed.
- **Align the sampling window with marker files, not breakpoints.** A
  software breakpoint planted while the target is stopped from a SIGINT does
  not fire after resume (two sessions lost to this before it was understood).
  Instead, have the scenario Lua touch a marker file when the event of
  interest happens, and let the driver free-run — no breakpoints armed at
  all — polling for the file before it starts sampling.
- **Sampling answers "where", never "how long".** Every stop/resume delivers
  a backlog of guest events that lands as real work once resumed — update
  events become paints — and the sampler will then catch work of its own
  making. One investigation reported a 13-second busy window that a
  free-running A/B run showed did not exist. Duration claims come only from
  free-running measurements — a scenario Lua taking `screen:snapshot` on a
  fixed cadence with no debugger attached — and the sampler's role is limited
  to attributing a window whose duration was established without it.

## Verification status

- macOS Tahoe: runtime-verified on Intel and Apple silicon (A18 Pro) with MAME
  0.288 on a 68030 machine, including live floppy insertion and the generated
  `LokaDev` SCSI volume.
- Windows through WSL: runtime-verified with MAME 0.287 on a 68030 machine,
  including the combined Retro68 build, generated `LokaDev` SCSI disk, and
  host-side MAME startup from a WSL-hosted VS Code window.
- Source-level gdb debugging: runtime-verified on Windows on ARM through WSL,
  with a native Aarch64 MAME 0.287 on a 68030 machine. The emulated machine ran at
  roughly 6.7x real time, so a full boot-and-launch cycle takes about two
  minutes.
