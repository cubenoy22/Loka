# macOS Build Scripts

`scripts/macos` provides entry points by target OS generation.
In normal use, call one of the wrapper scripts below instead of `build.sh` directly.

## Which Script To Run

- `scripts/macos/build-10_4.sh`
  - Legacy build path for Tiger/Leopard compatibility work.
  - Defaults: `DEPLOYMENT_TARGET=10.4`, `ARCHS=ppc;i386`, `MAC_OS_10_4=1`
  - Intended for environments with the 10.4u SDK installed.

- `scripts/macos/build-10_7.sh`
  - Standard build path for Lion and newer.
  - Defaults: `DEPLOYMENT_TARGET=10.7`, `MAC_OS_10_4=0`

- `scripts/macos/build.sh`
  - Shared implementation used by the two wrapper scripts.
  - Use directly only when you need fully custom environment variables.

## Examples

```bash
# 10.7+ (default path)
./scripts/macos/build-10_7.sh

# 10.4 target (ppc + i386)
./scripts/macos/build-10_4.sh

# 10.4 target for one app target only
TARGET=LokaSimpleViewerMacOS ./scripts/macos/build-10_4.sh
```

## Main Environment Variables

- `BUILD_TYPE` (default: `Release`)
- `TARGET` (if omitted, builds default/all configured targets)
- `ARCHS` (`;` separated, for example `ppc;i386` or `x86_64`)
- `GENERATOR` (`Ninja` / `Unix Makefiles`)
- `MAC_OS_10_4_SYSROOT` (example: `/Developer/SDKs/MacOSX10.4u.sdk`)
- `CC`, `CXX` (for example `gcc-4.0`, `g++-4.0` when needed)
