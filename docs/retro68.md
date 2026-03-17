# Retro68 setup

This project supports building Classic Mac OS targets with Retro68.

## Requirements

- Retro68 toolchain installed.
- `retro68.toolchain.cmake` available in a toolchain directory.

## Configuration

This repo includes shared Retro68 presets in `CMakePresets.json`:

- `retro68-default-release`

These presets use `cmake/toolchains/Retro68.cmake`, which resolves the toolchain
from environment variables or common default paths.

### Option A: Use shared presets directly

Set one of the following so the toolchain can be found:

- `RETRO68_TOOLCHAIN_DIR` (path to Retro68 toolchain CMake dir or toolchain file)
- `RETRO68_BUILD_DIR` (path to Retro68 build output directory)

Then run:

```sh
cmake --preset retro68-default-release
cmake --build --preset retro68-default-release
```

### Option B: Add local `CMakeUserPresets.json`

If you want machine-specific overrides (for example custom `PATH`), create
`CMakeUserPresets.json` in the project root:

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "retro68-debug",
      "displayName": "Retro68 (Debug)",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/retro68/Debug",
      "environment": {
        "PATH": "/path/to/Retro68-build/toolchain/bin:$penv{PATH}"
      },
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/cmake/toolchains/Retro68.cmake",
        "RETRO68_BUILD_DIR": "/path/to/Retro68-build"
      }
    },
    {
      "name": "retro68-release",
      "displayName": "Retro68 (Release)",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/retro68/Release",
      "environment": {
        "PATH": "/path/to/Retro68-build/toolchain/bin:$penv{PATH}"
      },
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_TOOLCHAIN_FILE": "${sourceDir}/cmake/toolchains/Retro68.cmake",
        "RETRO68_BUILD_DIR": "/path/to/Retro68-build",
        "CMAKE_CXX_FLAGS_RELEASE": "-Os -ffunction-sections -fdata-sections -fno-exceptions",
        "CMAKE_EXE_LINKER_FLAGS_RELEASE": "-Wl,--gc-sections"
      }
    }
  ]
}
```

Replace `/path/to/Retro68-build` with your actual Retro68 build directory.

## Building

```sh
cmake --preset retro68-default-release
cmake --build --preset retro68-default-release --target LokaHelloClassic
```

Output files:
- Shared preset default:
  - `build/retro68/Release/example/HelloWorld/LokaHelloClassic.dsk`
  - `build/retro68/Release/example/HelloWorld/LokaHelloClassic.bin`
- If you use custom `CMakeUserPresets.json`, paths follow your `binaryDir`.

## Environment variables (alternative)

If you prefer environment variables over CMakeUserPresets.json:

- `RETRO68_BUILD_DIR`: path to the Retro68 build output directory.
- `RETRO68_TOOLCHAIN_DIR`: path to the Retro68 toolchain CMake directory.
- `RETRO68_CPU`: target CPU (`m68k` or `ppc`). Defaults to `m68k`.
- `RETRO68_PPC_FLAVOR`: `retroppc` or `retrocarbon` (when `RETRO68_CPU=ppc`).

## Notes

- Retro68 builds use the `apple/toolbox` target.
- Release builds use `-Os` and section garbage collection for smaller binaries.
- The `LOKA_RETRO68` preprocessor define is set for Classic Mac builds.
- If a `.dsk` is mounted, unmount it before rebuilding. Mounted images can leave stale artifacts or misleading runtime results.

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

## VSCode (IntelliSense)

To enable Classic Mac OS headers in IntelliSense, add the Retro68 interfaces
directory to your include path.

This repo includes a starter configuration at:

- `.vscode/c_cpp_properties.json`

Update the path if your Retro68 build directory differs from `../Retro68-build`.
