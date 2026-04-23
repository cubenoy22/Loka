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
  - Arch and deployment target are controlled by env vars (default `ARCHS=x86_64`, `DEPLOYMENT_TARGET=10.8`).
  - The default is intentionally conservative so the generated project stays easier to reuse across older Apple IDEs.
  - Requires a full `Xcode.app` installation; Command Line Tools alone are not enough.
  - Intended for OS X Mountain Lion (10.8) and later.
  - Projects generated on 10.8+ are generally Xcode 3.2-compatible enough to be opened on Snow Leopard with Xcode 3.2.6.
  - Some newer flags may still need manual cleanup for Xcode 3.2.6, for example removing unsupported options such as `-fno-objc-arc`.
  - The generated project remains path-sensitive: source references are anchored to the path used when `gen-xcodeproj.sh` ran.
  - In practice, older Xcode versions may fail to resolve sources if the repository root folder name changes or if the project is reopened from a different absolute path after generation.
  - Treat generated `build/macos-xcodeproj/...` projects as machine-local artifacts; regenerate them on the target machine/path instead of copying them as reusable nightly-build artifacts.
  - Legacy `MAC_OS_10_4*` flags are intentionally not used here.

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

# Generate Xcode project for x86_64 only
ARCHS="x86_64" DEPLOYMENT_TARGET=10.8 ./scripts/macos/gen-xcodeproj.sh
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
