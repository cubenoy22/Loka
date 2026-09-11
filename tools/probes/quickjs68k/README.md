# QuickJS 68K engine probe

Standalone measurement host for `JS_Eval("1+1")`, using the same pinned
QuickJS-ng v0.16.2 four-source target as SmirkyCard. It has no Loka Scene,
Window, UI DSL, or dependency on issue #657. Normal builds do not include it.
This is a portability experiment, not a supported Classic runtime library.

## Build and run

From the repository root, with the normal `.env-retro68` host configuration:

```sh
scripts/retro68-cmake.sh -S tools/probes/quickjs68k -B build/quickjs68k -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/Retro68.cmake" \
  -DRETRO68_CPU=m68k -DCMAKE_BUILD_TYPE=Release
cmake --build build/quickjs68k -j 4
```

Python 3.9+ is needed only to prepare this experiment's dependency copy.
The pinned source downloads automatically. An offline copy can be selected
with `-DFETCHCONTENT_SOURCE_DIR_SMIRKYCARD_QUICKJS=/absolute/source/path`.
The copy must be the unmodified pinned source. All patched/downloaded sources
and generated applications stay under `build/`; the shared cache is unchanged.
Unmount any previous disk image before rebuilding.

Mount `build/quickjs68k/LokaQuickJSProbe.dsk` in MAME, or transfer
`build/quickjs68k/LokaQuickJSProbe.bin` as MacBinary to the Classic machine.
Launch **LokaQuickJSProbe**. Pass means `result=2 status=PASS`,
`released: payload_live=0`, and `final=PASS`; Return closes the console.
Record the complete console output, machine, OS, RAM, and partition size.
Runtime/context allocation refusal reports failure and also waits for Return.
If the application cannot launch, record that as a partition/runtime failure,
not an engine evaluation result.

The SIZE resource requests a 2 MiB minimum/preferred application partition.
That is a measurement setting, not a claim that it fits alongside System 7
on a 4 MB machine. The engine limit is 512 KiB (set after runtime creation);
the JS stack guard is 32 KiB and does not reserve OS stack space. Nothing
changes SmirkyCard's current limits.

## What is measured

- `payload_live`: sum of `GetPtrSize` over live allocations through the probe's
  `JS_NewRuntime2` allocator, including runtime/context allocation.
- `payload_peak`: maximum of that sum at every successful allocation. Realloc
  deliberately allocates/copies/releases, so its temporary double occupancy is
  included; refusal keeps the original block. These are allocator payloads,
  **not** Memory Manager headers, static tables, stack, code, or total app RAM.
- `heap_free`: `FreeMem` snapshot at each named phase, **not** an OS heap high
  water measurement. Console/stdio allocation may affect these snapshots.
- The pinned engine's own memory limit includes its accounting overhead and
  need not equal `payload_live`. Initial runtime creation precedes that limit.

Keep all these quantities separate. In particular, libunicode's compiled
read-only tables are not automatically equivalent to JS heap allocations.
Only Classic execution can supply the missing peak and partition evidence.

## Compatibility changes

Unmodified upstream sources fail with this Retro68 GCC 16.1.0 setup:

- `__STDC_NO_ATOMICS__=1` disables Atomics, but cutils still selects pthread
  wrappers; Classic has no matching declarations.
- The monotonic timer assumes `clock_gettime` / `CLOCK_MONOTONIC`.
- `int32_t` is `long`, while four local declarations pass `int *` to
  `int32_t *` APIs. Both types are 32 bits, but they are distinct C types.

`prepare_source.py` makes exact-checked edits in a build-local copy: select
cutils' existing no-thread branch, route the monotonic clock to Toolbox
`Microseconds`, and use `int32_t` for those locals. No fake pthread functions,
diagnostic suppression, regexp removal, or Unicode removal is used.
The timer and NewPtr allocator are probe-local platform seams. This does not
establish Date/timezone correctness or general JavaScript compatibility.

## Evidence (2026-09-11)

Base `de755699` (#654), Retro68 GCC 16.1.0, Universal Interfaces, Release with
`-Os -ffunction-sections -fdata-sections`, linker GC and MacsBug stripping.
**Build-verified only** on 68K. No MAME or hardware run has been performed.

| Measurement | Bytes |
| --- | ---: |
| Final MacBinary | 1,466,752 |
| CODE payload total (9 resources, including loader) | 1,311,196 |
| DATA payload total | 113,164 |
| RELA payload total | 41,372 |
| Engine archive file (includes metadata; not installed code size) | 1,170,756 |
| quickjs.c object text, before final link GC | 492,122 |
| dtoa.c object text | 8,020 |
| libregexp.c object text | 23,760 |
| libunicode.c object text | 55,440 |

All four engine objects and the final application link, including dtoa and
64-bit integer support. Retro68 produced the CODE resources automatically;
no manual segmentation changes were needed. The complete console application
is about 1.40 MiB, while engine object text totals about 566 KiB. Do not call
the entire application size the engine's size. Inspect the final ELF to
attribute console, C/C++ runtime, and math overhead before choosing features
to remove. Disabling unwind tables on the C engine was tried and did not
reduce the final artifact; those flags were dropped.

Reproduce payload totals using the existing repository parser:

```sh
python3 - <<'PY'
import pathlib, sys
sys.path.insert(0, 'tools/ci')
from retro68_size_report import resource_payload_sizes
p = pathlib.Path('build/quickjs68k/LokaQuickJSProbe.bin')
print('MacBinary', p.stat().st_size, resource_payload_sizes(p))
PY
```

The toolchain's `m68k-apple-macos-size` reports the four objects under
`build/quickjs68k/CMakeFiles/smirkycard_quickjs.dir/classic-source/`.
Use `m68k-apple-macos-nm --size-sort -S` on
`build/quickjs68k/LokaQuickJSProbe.code.bin.gdb` for link attribution.

A Linux host build of this same evaluation/lifecycle host returns 2 and PASS:

```sh
cmake -S tools/probes/quickjs68k -B build/quickjs-host -G Ninja \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build/quickjs-host -j 4
build/quickjs-host/LokaQuickJSProbe
```

It explicitly skips the Classic allocator measurements. It is not evidence
for the Classic allocator, timer, stack, or target runtime behavior. Pending
acceptance: actual 68K evaluation, allocation peak, zero remaining payload,
and successful launch at the stated partition on the named 4 MB hardware.
No decision to strip engine features or wire a Toolbox UI is made yet.
