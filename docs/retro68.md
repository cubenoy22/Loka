# Retro68 setup

This project supports building Classic Mac OS targets with Retro68.

## Requirements

- Retro68 toolchain installed.
- `retro68.toolchain.cmake` available in a toolchain directory.

## Configuration

This repo includes shared Retro68 Release presets in `CMakePresets.json`:

- `retro68-68k-release`
- `retro68-ppc-release`
- `retro68-68k-standalone-release`
- `retro68-ppc-standalone-release`

These presets use `cmake/toolchains/Retro68.cmake`, which resolves the toolchain
from host-local configuration, environment variables, or common default paths.

By default, the Retro68 toolchain wrapper checks:

- `~/Retro68-build`
- `~/Retro68`

If your Retro68 build is in one of those locations and `ninja` is on `PATH`, the
shared Release presets can be used directly. Otherwise, copy the checked-in
example and describe this host once:

```sh
cp .env-retro68.example .env-retro68
```

`.env-retro68` is ignored by Git. VS Code Retro68 tasks and
`scripts/retro68-cmake.sh` load it automatically on macOS, Linux, and WSL. Use
paths as seen by the current host; a WSL file therefore contains Linux or
`/mnt/...` paths, not Windows path syntax.

For Retro68, use the Release presets directly. Dedicated `Debug` or
`MinSizeRel` presets are not maintained here because the practical workflow is
already Release-oriented and the Release flags are the ones we want to keep
consistent across Classic targets.

### Redistributable interfaces in hosted CI

The `Toolbox Build CI` workflow builds every Toolbox target for both 68K and
PowerPC in Retro68's official container. It selects the redistributable
Multiversal Interfaces explicitly and enables Loka's internal compatibility
boundary with `LOKA_TOOLBOX_MULTIVERSAL_INTERFACES=ON`. The container image is
pinned by digest in `.github/workflows/toolbox.yml` so the compiler and header
set cannot change silently.

This workflow is **build-verified** only: it proves that the applications
compile and link without redistributing Apple's Universal Interfaces. Runtime
and golden verification remain on the named Toolbox rig described in
`docs/LOKA_RIG.md`.

Universal Interfaces remain the default for existing local Retro68 setups. To
reproduce the hosted interface choice inside a Retro68 container launched with
`INTERFACES=multiversal`, run:

```sh
cmake --preset retro68-68k-release -G "Unix Makefiles" \
  -DLOKA_TOOLBOX_MULTIVERSAL_INTERFACES=ON
cmake --build --preset retro68-68k-release
cmake --preset retro68-ppc-release -G "Unix Makefiles" \
  -DLOKA_TOOLBOX_MULTIVERSAL_INTERFACES=ON
cmake --build --preset retro68-ppc-release
```

The compatibility headers under `apple/toolbox/compat/multiversal` are a
Toolbox implementation detail. They preserve the Universal-style contracts
already used by the backend and contain Multiversal's global Toolbox names;
they are not a second app-facing API.

### Host configuration

The local file accepts these settings:

- `RETRO68_BUILD_DIR` (path to Retro68 build output directory)
- `RETRO68_TOOLCHAIN_DIR` (advanced override for one CPU's toolchain CMake
  directory or file)
- `RETRO68_TOOLCHAIN_BIN` (optional host-tool directory)
- `CMAKE_MAKE_PROGRAM` (optional path to Ninja)

Prefer `RETRO68_BUILD_DIR`: the toolchain file uses it to select the matching
68K or PPC directory for each preset. `RETRO68_TOOLCHAIN_DIR` points at one
CPU's toolchain CMake directory, so setting it pins configuration to that CPU
and makes the other preset family fail. Set `CMAKE_MAKE_PROGRAM` when an editor
launched from the desktop cannot see Ninja on its inherited `PATH`. Values
already supplied by a caller or CI take precedence over the local file.

Then run:

```sh
scripts/retro68-cmake.sh --preset retro68-68k-release
scripts/retro68-cmake.sh --build --preset retro68-68k-release
```

Report the final 68K MacBinary application sizes after the build with:

```sh
python3 tools/ci/retro68_size_report.py build/retro68/68k/Release
```

The report reads each final `.bin` resource fork and prints its total file size
and CODE/DATA/RELA payload composition. The checked-in baseline manifest owns
the shipping application list and allows at most 4096 bytes of total growth per
application before the command fails. Component deltas are always printed for
attribution. The 4 KiB allowance ignores small toolchain/resource-alignment
churn while still catching the multi-kilobyte framework expansions that
motivated the size audit. It is cumulative from the checked-in baseline, so a
series of smaller increases cannot silently reset the allowance: only a
deliberate, reviewed baseline refresh moves the reference point. A deliberate
baseline refresh is its own reviewed change, or rides the change that introduces
the growth with the explanation in that PR. The command also refuses a newly
built final application that has no explicit baseline entry. Toolbox CI runs
the same command after its pinned 68K build. Update baseline facts only with a
fresh full Release build and a reviewed explanation for the accepted growth.
Per the 2026-09-02 ruling the
gate is a drift detector, not a budget: intentional feature growth is banked by
refreshing rows deliberately, and only runaway growth — hundreds of kilobytes
on a tens-of-kilobytes-class application — would justify a hard ceiling.

The preset name describes the target and build policy, not the host. There is
no separate `local` preset: every host uses the same repository Release preset,
so the Classic size and linker policy cannot drift between local and shared
builds. Direct `cmake` invocations do not load dotenv files; use the wrapper, or
export the same variables explicitly in an already-managed CI environment.

## Building

Use the repository Release preset through the wrapper:

```sh
scripts/retro68-cmake.sh --preset retro68-68k-release
scripts/retro68-cmake.sh --build --preset retro68-68k-release --target LokaHello68K
```

Output files:
- `retro68-68k-release`:
  - `build/retro68/68k/Release/example/HelloWorld/LokaHello68K.dsk`
  - `build/retro68/68k/Release/example/HelloWorld/LokaHello68K.bin`
- `retro68-ppc-release`:
  - `build/retro68/ppc/Release/example/HelloWorld/LokaHelloPPC.dsk`
  - `build/retro68/ppc/Release/example/HelloWorld/LokaHelloPPC.bin`

For transportable autonomous Standalone releases, run the corresponding VS
Code task **Standalone: Toolbox 68K Release Action** or **Standalone: Toolbox
PPC Release Action**. Release artifacts are staged under
`build/release/toolbox-68k` or `build/release/toolbox-ppc` respectively.

## Environment variables

The CMake toolchain also accepts these variables from an explicitly managed
process environment:

- `RETRO68_BUILD_DIR`: path to the Retro68 build output directory.
- `RETRO68_TOOLCHAIN_DIR`: advanced override for one CPU's Retro68 toolchain
  CMake directory or file. It pins configuration to that CPU and makes the
  other preset family fail; prefer `RETRO68_BUILD_DIR` for shared 68K and PPC
  use.
- `RETRO68_CPU`: target CPU (`m68k` or `ppc`). Defaults to `m68k`.
- `RETRO68_PPC_FLAVOR`: `retroppc` or `retrocarbon` (when `RETRO68_CPU=ppc`).

The host-local file intentionally accepts only the location variables listed
under Host configuration. CPU and flavor remain owned by the selected preset.

Preset mapping used in this repo:

- `retro68-68k-release`: `RETRO68_CPU=m68k`
- `retro68-ppc-release`: `RETRO68_CPU=ppc`, `RETRO68_PPC_FLAVOR=retroppc`

### Compact and diagnostic 68K profiles

`retro68-68k-release` is the compact shipping profile. It strips Classic
MacsBug names from the application resources, compiles application targets
without RTTI, and leaves function-tick profiling and diagnostic file I/O out of
the binary. A separate DWARF-bearing `.gdb` link artifact remains available.

`retro68-68k-diag` is the diagnostic profile. It retains MacsBug names, enables
the function-tick profiler and diagnostic dumps, and does not define `NDEBUG`.
It is **build-verified only**; no runtime verification is claimed for that
profile. The compact 68K profile is runtime-verified separately on the named
Classic rigs; each runtime claim must identify the exact artifact and evidence
path rather than inheriting the diagnostic profile's build result.

## Notes

- Retro68 builds use the `apple/toolbox` target.
- Release builds use `-Os` and section garbage collection for smaller binaries.
- The `LOKA_RETRO68` preprocessor define is set for Classic Mac builds.
- If a `.dsk` is mounted, unmount it before rebuilding. Mounted images can leave stale artifacts or misleading runtime results.
- 68k support remains important for compatibility and experimentation, and simple samples can still boot on 68000-class environments.
- In practical terms, startup cost and responsiveness become much more constrained on low-end 68k (68000 / 68020) as applications grow in size and complexity.
- For modern-style application development on Classic Mac OS, 68030-class systems and later (and PPC601 / 603e-class PowerPC Macs) should be treated as the practical mainstream baseline; all bundled examples are runtime-verified on a 68030 PowerBook 180c with 4 MB of RAM.

## Header Boundary Notes

Classic Mac OS headers expose many old Toolbox names in the global namespace.
For example, both Universal Interfaces and Retro68's Multiversal headers can
declare a global `Button()` function. If those headers become visible in a
translation unit that also uses `using namespace loka::app`, an unqualified Loka
DSL expression such as `Button("OK")` can become ambiguous.

The important fix is not to make application authors qualify every Loka UI
control. App-facing composition should stay written in Loka concepts. Keep
Classic headers inside Toolbox implementation files, platform-specific bootstrap
files, or other private backend seams.

One subtle source of leakage is standard C/C++ compatibility headers in the
Retro68 toolchain. In at least one Retro68 setup, including `<cstring>` from a
public Loka header pulled in `string.h`, then Classic string headers, then
`Events.h`, which made the global Toolbox `Button()` visible to ordinary app
composition code. Prefer typed operations or small internal helpers over
public-header includes when the only need is a tiny standard-library operation.

## Classic Toolbox Profiles

Classic Mac localization should not assume that Unicode is the only normal
answer. For pre-8.5 Toolbox-era targets, region-specific builds and resources
are often more natural and stable than forcing one Unicode-first runtime path.

The likely long-term shape is:

- `ToolboxEN`: English/Roman-oriented Classic Toolbox resources and text path.
- `ToolboxJP`: Japanese Classic Toolbox resources and text path, similar in
  spirit to traditional `1.0J` builds.
- `iToolbox`: a Mac OS 8.5+ "internet-era" Toolbox profile that may use
  Unicode/ATSUI-style capabilities and newer Classic-era text/resource
  behavior where available.

The names are not final. The important rule is that Loka's app-facing
`String`, `AssetPool`, and resource APIs should stay neutral while Classic
backend profiles choose the appropriate build-time resource set and text
backend. Japanese localization for Toolbox should wait until the String and
asset ownership model is solid enough that the profile split is an implementation
choice rather than an app-facing API fork.

## Debug Workflow

Use different tools for different failure classes.

### 1. Modern-host profiling first

For performance problems, first reproduce on a modern host build and profile there.

- macOS: use `Time Profiler`
- Look for the top hot path before touching code
- Typical order:
  1. reproduce
  2. capture top symbols
  3. isolate by toggling sample features or commenting out components
  4. optimize only the top hotspot
  5. re-measure

This was effective for distinguishing:

- reconciler/local-diff cost
- state propagation fanout
- backend redraw cost

### 2. Classic crash debugging

For Classic-only crashes, use native Mac OS with MacsBug.

Recommended setup:

- OS 9 native on PPC hardware is preferred over Classic mode
- `MacsBug` installed
- Retro68-built `.dsk` / `.APPL`

Why:

- close/teardown crashes are often Toolbox/TE/Control Manager specific
- Classic mode adds another compatibility layer and makes root-cause analysis noisier

### 3. When to switch to MacsBug

Switch to MacsBug when the problem is:

- `illegal instruction`
- `CHK`
- `bad F-Line`
- bus error / address error
- close/quit-only crash
- crash that does not reproduce on modern macOS/Win32/Linux

### 4. Minimal MacsBug workflow

For a crash, record at least:

- crashing symbol / offset
- `sc6`
- `es` if it is still valid

In this project, that was enough to isolate a Classic close crash to:

- `ToolboxScenePlatformController::clearTextBindings()`

### 5. Practical rule

Do not guess for long on Classic teardown bugs.

- modern host + `Time Profiler` for performance
- OS 9 + `MacsBug` for Classic crash points
- if needed, ask someone with native hardware to reproduce and capture the crash symbol/stack

That hybrid workflow is faster than trying to solve all Classic issues from Linux/macOS alone.

## Classic animation / redraw notes

These notes came from profiling and tightening the `FloppyBird` sample on
Toolbox/68k.

### 1. Measure update routing before dirty-region tricks

For animated UI on Classic, broad repaint is not always the main cost.
In `FloppyBird`, the first large wins came from reducing:

- redundant `MutableState::set(..., true)` usage
- unused state writes
- extra `Boundary` / compose layers
- idle ticks that did not produce a visible output change

Only after those were removed did redraw cost become the top hotspot.

Practical rule:

- profile first
- remove redundant state/compose work before adding redraw complexity

### 2. Quantized-output gating is effective

When output is rendered at integer coordinates, game logic can advance without
producing a visible change every tick.

In `FloppyBird`, a cheap "render snapshot" of quantized sprite positions was
enough to skip model rebuild / `set()` / compose work for frames where the
visible output had not changed yet.

Practical rule:

- if a Classic animation ultimately snaps to integer positions, compare the
  quantized output first
- if nothing visible changed, skip rebuilding props/models and avoid notifying
  state

### 3. Keep surface-level optimizations in `NodeContext`

The stable path was:

- keep `RectSurfaceNode` as a normal Node
- keep shared app/model data simple
- store previous rendered state inside the Toolbox `NodeContext`
- perform Classic-specific erase/paint decisions there

This kept platform-specific optimization out of the shared DSL/app layer and
made future backends (for example CALayer-backed macOS paths) easier to reason
about.

### 4. Prefer `erase old minus new` before aggressive paint diffing

For moving rectangles, the most reliable first optimization was:

- erase only the old pixels no longer covered by the new rect
- then paint the current rect normally

This removed flicker and reduced erase area without the instability of more
aggressive "paint new minus old" matching.

Practical rule:

- first try `erase old minus new`
- only add paint-side diffing after profiling proves it helps

### 5. `Rgn`/region clipping can lose to simpler paths

`Rgn`-based clipping improved visual synchronization in some experiments, but
once redundant update/compose work was removed, it became slower than a simpler
rect-based path for this workload.

Practical rule:

- do not assume `Rgn` is faster
- measure it after higher-level update work has already been reduced
- keep it as a backend-local policy, not a general DSL feature, unless repeated
  measurements justify exposing it

## Classic investigation notes

These notes come from stabilizing repeated `PopupMenu` crashes in the
`StaticVsDynamicClassic` sample on System 7.1 / 9.1.

### 1. Do not over-attribute crashes to "old OS instability"

Classic Mac OS is fragile, but repeated crashes under the same interaction
pattern usually still have a concrete cause.

In this project, repeated `Toggle Details` / `Freeze` stress initially looked
like generic Classic instability. It was not. The root cause was a persistent
popup context design problem in the Toolbox backend.

Practical rule:

- if the crash is reproducible within tens or hundreds of actions, assume there
  is a real bug until proven otherwise
- use the old environment as a stress amplifier, not as an excuse to stop
  debugging

### 2. Cut the problem with sample variants first

Before changing core code, create narrow diagnostic variants of the sample and
 compare stability:

- no native controls
- no `EditText`
- no `PopupMenu`

This was decisive:

- `ClassicLight`: stable
- `NoPopupMenu`: stable
- `NoEditText`: unstable

That isolated the problem to `PopupMenu`, not to the reconciler or dynamic
boundary logic.

Practical rule:

- when a Classic crash looks "random", first remove whole control classes and
  compare runtime behavior
- this is often faster than stepping through low-level crashes immediately

### 3. Prefer frame-local snapshots over persistent UI-context registries

The key Classic `PopupMenu` bug was caused by keeping persistent
`ToolboxPopupMenuContext*` objects in a controller-managed vector and reusing
them across redraw/input cycles.

The stable fix was:

- stop registering popup contexts into a persistent controller loop
- replace them with frame-local popup hit snapshots
  - rect
  - line height
  - items pointer
  - selected-index state
  - enabled state
  - onChange emitter
  - boundary
  - menu id

This significantly improved stability on:

- mini vMac / System 7.1
- native OS 9 PPC hardware

Design rule:

- on Classic Toolbox paths, do not retain more live native UI context than you
  need
- if a control can be described by a frame-local snapshot for hit-testing and
  redraw, prefer that over a long-lived controller-side registry

### 4. Persistent backend context is especially suspicious when

Watch for backend-specific registries that hold:

- raw node/context pointers
- state pointers
- emitters

## Classic redraw investigation notes

These notes come from profiling `StaticVsDynamicClassic` redraw behavior on
mini vMac/System 7.1 and native PPC OS 9.

### 1. Split startup cost from interaction cost

Do not mix these into one conclusion.

- startup can show repeated full draws even before any user action
- interaction cost can be much lower after core fixes, but still look bad if
  startup/event-loop noise is included in the same measurement window

Use separate captures:

- startup: launch, then dump immediately
- interaction: reset, perform one fixed action, then dump

### 2. Root dynamic host boundary materially changed the result

For `StaticVsDynamic`, switching the root host to a dynamic boundary removed the
main `fullRebuild -> full invalidate` path for `Toggle Details`.

Observed result after that change:

- `full_rebuild=0`
- `request_full=0`
- `request_rect=1`

That was the largest meaningful performance win in this phase.

Practical rule:

- if a sample mixes static host composition with dynamic child boundaries and
  repeatedly toggles child structure, test a dynamic host root before spending
  too long on backend redraw policy

### 3. Dynamic pane and startup/event-loop noise are separate problems

When the dynamic pane was temporarily removed from `StaticVsDynamicClassic`:

- the second `Profile Toggle x2` dump became much cleaner
- rect-path redraw was still visible
- `flush_full` could drop to `0` for the second pass

This means:

- dynamic-pane interaction noise is real
- but startup/update-event full draws are a different issue

Do not treat them as one bug.

### 4. Dump chaining is useful, but completion semantics matter

`Profile Toggle x2` was only reliable after dump completion was chained through
an explicit deferred path.

However, "dump finished" and "UI/event-loop is stable enough for the next
action" are not the same thing on Classic Toolbox.

Practical rule:

- use chained dumps for comparison
- but do not assume a deferred callback means redraw/update events have fully
  drained

### 5. Prefer simple sample subtraction before deeper backend work

When redraw profiling gets muddy, remove whole sample slices first:

- dynamic pane
- popup/edit controls
- status/count text updates

If removing a slice materially changes the redraw trace, that is more useful
than adding another round of backend speculation.
- layout rects

If they survive longer than one render pass, they are prime suspects for:

- illegal instruction
- type 3 / type 10
- bad F-Line
- silent exits

Practical review question:

- can this be rebuilt every frame from current scene state instead of being
  retained in a controller vector?

If yes, that design is usually safer on Classic.

### 5. Safe-side hardening is useful, but not always sufficient

The following changes were still worthwhile:

- invalidating popup raw pointers on teardown
- avoiding fixed menu IDs when a stable control tag exists
- removing per-draw `PolyHandle` churn
- snapshotting popup state around `PopUpMenuSelect()`

However, those did not fully solve the problem until the persistent popup
context registry itself was removed.

Practical rule:

- hardening measures help
- but if a crash remains after several safe-side fixes, question the ownership
  model, not only the cleanup details

## VS Code (clangd)

To enable Classic Mac OS headers in clangd, select a Retro68 preset in CMake
Tools and configure it. The Retro68 presets generate a compilation database,
and the tracked VS Code settings copy the active database to
`compile_commands.json` at the repository root for clangd.

The workspace permits clangd to query only the Retro68 68K and PPC C++ compiler
drivers named by that database. This supplies the toolchain's actual system
include paths without hard-coding the location of a local Retro68 build.

`.vscode/c_cpp_properties.json` remains as a starter configuration for anyone
who explicitly re-enables the Microsoft C/C++ IntelliSense engine.
