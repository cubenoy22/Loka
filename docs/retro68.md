# Retro68 setup

This project supports building Classic Mac OS targets with Retro68. Use the
CMake preset and configure environment variables per machine.

## Requirements

- Retro68 toolchain installed.
- `retro68.toolchain.cmake` available in a toolchain directory.

## Environment variables

- `RETRO68_TOOLCHAIN_DIR`: path to the Retro68 toolchain CMake directory.
- `RETRO68_BUILD_DIR`: path to the Retro68 build output directory (used for auto-detect).
- `RETRO68_CPU`: target CPU, set to `m68k` or `ppc`.

If your Retro68 build lives at `../Retro68-build`, the toolchain file is
auto-detected from one of:

- `../Retro68-build/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake`
- `../Retro68-build/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake`
- `../Retro68-build/toolchain/powerpc-apple-macos/cmake/retrocarbon.toolchain.cmake`

so you can omit `RETRO68_TOOLCHAIN_DIR` when using those layouts.

Example (Unix shells):

```sh
export RETRO68_TOOLCHAIN_DIR=/path/to/Retro68/cmake
export RETRO68_CPU=ppc
cmake --preset retro68-base
```

Example (PowerShell):

```powershell
$env:RETRO68_TOOLCHAIN_DIR="C:\Retro68\cmake"
$env:RETRO68_CPU="ppc"
cmake --preset retro68-base
```

## Notes

- Retro68 builds are expected to use the `apple/toolbox` target.
- `RETRO68_CPU` can be refined later to support specific CPU variants once the
  toolchain flags are confirmed.
- `../Retro68-build/toolchain/bin` can be added to `PATH` if you want to run
  toolchain commands directly.
- `RETRO68_PPC_FLAVOR` selects `retroppc` or `retrocarbon` when `RETRO68_CPU=ppc`.
