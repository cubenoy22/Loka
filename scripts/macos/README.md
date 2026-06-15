# macOS Build Scripts

`scripts/macos` provides entry points by target OS generation.
In normal use, call one of the wrapper scripts below instead of `build.sh` directly.

## Which Script To Run

- `scripts/macos/build-10_4.sh`
  - Legacy build path for Tiger/Leopard compatibility work.
  - Defaults: `DEPLOYMENT_TARGET=10.4`, `ARCHS=ppc;i386`, `MAC_OS_10_4=1`
  - Intended for environments with the 10.4u SDK installed.
  - `MAC_OS_10_4_SYSROOT` is auto-resolved from `DEVELOPER_DIR` / `xcode-select` when unset.
  - `ppc` builds require a working PPC-capable compiler/toolchain (validated by a compile check).
  - If PPC tools are missing, the build fails by default; set `ALLOW_PPC_FALLBACK=1` to continue with non-ppc archs.
  - Outputs are written under `build/macos-10.4-ub1`.

- `scripts/macos/build-10_5.sh`
  - Leopard multi-arch path excluding `ppc64`.
  - Defaults: `DEPLOYMENT_TARGET=10.5`, `ARCHS=ppc;i386;x86_64`, `MAC_OS_10_4=0`
  - Uses `OSX_SYSROOT=/Developer/SDKs/MacOSX10.5.sdk` by default.
  - By default builds app targets only (`LokaFloppyBirdMacOS`, `LokaHelloMacOS`, `LokaMineMacOS`, `LokaSimpleViewerMacOS`, `LokaTutorialMacOS`) and creates merged outputs in `build/macos-10.5-ub1/universal`.

- `scripts/macos/build-10_7.sh`
  - Standard build path for Lion and newer.
  - Defaults: `DEPLOYMENT_TARGET=10.7`, `MAC_OS_10_4=0`

- `scripts/macos/build-ub2.sh`
  - Universal Binary 2 path for modern macOS.
  - Defaults: `DEPLOYMENT_TARGET=11.0`, `ARCHS=arm64;x86_64`, `MAC_OS_10_4=0`
  - By default builds app targets only (`LokaFloppyBirdMacOS`, `LokaHelloMacOS`, `LokaMineMacOS`, `LokaSimpleViewerMacOS`, `LokaTutorialMacOS`).

- `scripts/macos/build.sh`
  - Shared implementation used by the two wrapper scripts.
  - Use directly only when you need fully custom environment variables.
  - On legacy gcc-4.x toolchains, use single-arch only (`ARCHS` must not contain `;`).

- `scripts/macos/gen-xcodeproj.sh`
  - Generates an Xcode project (`-G Xcode`) for debugging.
  - Arch, SDK, and deployment target are controlled by env vars (default `ARCHS=xcode-standard`, `OSX_SYSROOT=macosx`, `DEPLOYMENT_TARGET=11`).
  - The default uses Xcode's `$(ARCHS_STANDARD)` preset so modern Xcode versions choose the appropriate standard architecture set.
  - `ARCHS=xcode-standard` omits `CMAKE_OSX_ARCHITECTURES` and writes Xcode's `$(ARCHS_STANDARD)` preset into the generated project.
  - The legacy spelling `ARCHS=archs-standard` is accepted as an alias for `ARCHS=xcode-standard`.
  - Use an explicit CMake architecture list only when needed, for example `ARCHS="arm64;x86_64"` or `ARCHS=x86_64`.
  - Requires a full `Xcode.app` installation; Command Line Tools alone are not enough.
  - Intended for macOS Big Sur (11.0) and later by default.
  - For OS X Mountain Lion (10.8) and later, override `ARCHS` and `DEPLOYMENT_TARGET` explicitly.
  - Projects generated on 10.8+ are generally Xcode 3.2-compatible enough to be opened on Snow Leopard with Xcode 3.2.6.
  - Some newer flags may still need manual cleanup for Xcode 3.2.6, for example removing unsupported options such as `-fno-objc-arc`.
  - The generated project remains path-sensitive: source references are anchored to the path used when `gen-xcodeproj.sh` ran.
  - In practice, older Xcode versions may fail to resolve sources if the repository root folder name changes or if the project is reopened from a different absolute path after generation.
  - Treat generated `build/macos-xcodeproj/...` projects as machine-local artifacts; regenerate them on the target machine/path instead of copying them as reusable nightly-build artifacts.
  - Legacy `MAC_OS_10_4*` flags are intentionally not used here.

- `scripts/macos/gen-xcodeproj-10_6-ub1.sh`
  - Experimental Leopard/Snow Leopard Universal Binary 1 Xcode project generation path.
  - Defaults: `OSX_SYSROOT=macosx`, `DEPLOYMENT_TARGET=10.5`, `ARCHS=xcode-standard-32-64`.
  - Generates under `build/macos-xcodeproj-10.6-ub1/<build-type>-<arch-mode>`.
  - Requires a CMake/Xcode generator environment with Xcode 5.0 or newer; generate on a newer Mac and copy the project to Snow Leopard for Xcode 3.2.6 testing.
  - `ARCHS=xcode-standard-32-64` omits `CMAKE_OSX_ARCHITECTURES` and writes Xcode's `$(ARCHS_STANDARD_32_64_BIT)` preset into the generated project for Snow Leopard/Xcode 3.2.6 testing.
  - Use an explicit CMake architecture list only when needed, for example `ARCHS="i386;x86_64;ppc;ppc64"` on generator environments that still accept PPC architecture names.
  - Set `XCODE_ARCHS='$(ARCHS_STANDARD_32_64_BIT)'` only when overriding the default Xcode-native architecture preset; `ONLY_ACTIVE_ARCH=NO` is set by default.
  - Set `XCODE_VALID_ARCHS="i386 x86_64 ppc ppc64"` if the destination Xcode needs an explicit valid-architecture list.
  - Set `OSX_SYSROOT=macosx10.6`, `OSX_SYSROOT=macosx10.5`, or an SDK path when you need to force a specific SDK.
  - Disables Loka's explicit `-fno-objc-arc` example-target flags so the generated project is easier to open in older Xcode versions.
  - Suppresses generated `libarclite_macosx.a` link entries that are not available in Xcode 3.2.6.
  - Suppresses CMake's regeneration target so copied Snow Leopard projects do not require CMake just to build.
  - Sets `CLANG_ENABLE_OBJC_ARC=NO` for Xcode generators that understand it; Loka macOS code remains non-ARC by policy.
  - When opening in Xcode 3.2.6 for Leopard UB1 builds, manually add `ppc` and `ppc64` to Architectures if needed, set Base SDK to Mac OS X 10.5, and switch the compiler from GCC 4.2 to GCC 4.0.
  - This path is experimental: it is known to be friendlier on 10.8-era Xcode, should be retried on 10.10, and still needs 10.6/Xcode 3.2.6 verification.

## Examples

```bash
# 10.7+ (default path)
./scripts/macos/build-10_7.sh

# 10.4 target (ppc + i386)
./scripts/macos/build-10_4.sh

# 10.4 target for one app target only
TARGET=LokaSimpleViewerMacOS ./scripts/macos/build-10_4.sh

# 10.5 target (ppc + i386 + x86_64, no ppc64)
./scripts/macos/build-10_5.sh

# UB2 target (arm64 + x86_64, macOS 11+)
./scripts/macos/build-ub2.sh

# Generate Xcode project with Xcode's standard architecture preset
./scripts/macos/gen-xcodeproj.sh

# Generate Xcode project for x86_64-only older macOS debugging
ARCHS="x86_64" DEPLOYMENT_TARGET=10.8 ./scripts/macos/gen-xcodeproj.sh

# Generate experimental 10.6 SDK UB1 Xcode project
./scripts/macos/gen-xcodeproj-10_6-ub1.sh
```

## Main Environment Variables

- `BUILD_TYPE` (default: `Release`)
- `TARGET` (if omitted, builds default/all configured targets)
- `ARCHS` (`;` separated, for example `ppc;i386` or `x86_64`)
- `GENERATOR` (`Ninja` / `Unix Makefiles`)
- `MAC_OS_10_4_SYSROOT` (example: `/Developer/SDKs/MacOSX10.4u.sdk`)
- `OSX_SYSROOT` (example: `/Developer/SDKs/MacOSX10.5.sdk`)
- `CC`, `CXX` (for example `gcc-4.0`, `g++-4.0` when needed)
  - For `build-10_4.sh`, prefer `gcc-4.0` / `g++-4.0`; for `build-10_5.sh`, prefer `gcc-4.2` / `g++-4.2`.
