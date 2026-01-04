# Retro68 setup

This project supports building Classic Mac OS targets with Retro68.

## Requirements

- Retro68 toolchain installed.
- `retro68.toolchain.cmake` available in a toolchain directory.

## Configuration

Retro68 presets are machine-specific, so they are configured via `CMakeUserPresets.json` (not tracked in git).

Create `CMakeUserPresets.json` in the project root:

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "retro68-debug",
      "displayName": "Retro68 (Debug)",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build-retro68",
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
      "binaryDir": "${sourceDir}/build-retro68-release",
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
cmake --preset retro68-debug
cmake --build build-retro68 --target DeclaraHelloClassic
```

Output files:
- `build-retro68/example/HelloWorld/DeclaraHelloClassic.dsk` - Disk image for emulator
- `build-retro68/example/HelloWorld/DeclaraHelloClassic.bin` - BinHex file

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
