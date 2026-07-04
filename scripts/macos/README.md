# macOS Build Scripts

`scripts/macos` provides entry points by target OS generation.
In normal use, call one of the wrapper scripts below instead of `build.sh` directly.

## Verification Matrix

Legend: `:white_check_mark:` verified, `-` unchecked / not part of that route,
`:x:` checked and not working.

| Environment | Project generation | Native Xcode debug | Xcode 3.x UB1 build | Xcode 3.x debug | CLI build | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| Tiger 10.4 / Xcode 2.4 | - | :x: | - | :x: | - | Runtime-verified only for generated 10.4 UB1 samples; Xcode 2.4 debugging is not pursued. Tiger-hosted CLI builds are unchecked, including tigerbrew. |
| Leopard 10.5 / Xcode 3.1 | - | :white_check_mark: | :white_check_mark: | :white_check_mark: | :x: | Requires a copied project saved with Xcode 3.1 compatibility; CMake could not be installed via MacPorts in testing. |
| Snow Leopard 10.6 / Xcode 3.2.6 | - | :white_check_mark: | :white_check_mark: | :white_check_mark: | :white_check_mark: | Main UB1 bridge; copied projects and `build-10_4.sh` / `build-10_5.sh` are build-verified (`ppc/i386` and `ppc7400/i386/x86_64`). |
| Lion 10.7 + final Xcode / Snow Leopard Xcode 3.2.6 | - | :white_check_mark: | :white_check_mark: | - | :x: | Final Xcode can debug native builds; cross-partition Xcode 3.2.6 can build UB1 through the UI, but CLI wrappers cannot select it with `xcode-select`. |
| Mountain Lion 10.8 + final Xcode / Snow Leopard Xcode 3.2.6 | :white_check_mark: | :white_check_mark: | :white_check_mark: | - | :x: | Can generate projects from source, debug with the final Xcode, and run cross-partition Xcode 3.2.6 for UB1 UI builds. CLI wrappers cannot select that Xcode through `xcode-select`. |
| Mavericks 10.9 / Xcode 6.2 / Snow Leopard Xcode 3.2.6 | :white_check_mark: | - | :white_check_mark: | - | - | Legacy project generation works; Xcode 3.2.6 can launch there and build Leopard-facing four-architecture UB1 outputs from the UI. |
| Yosemite 10.10 | :white_check_mark: | :white_check_mark: | :x: | :x: | - | Project generation and native debugging work, but Xcode 3.2.6 cannot launch there. |
| El Capitan 10.11 through Sierra 10.12 / Xcode 9.2 | :white_check_mark: | :white_check_mark: | - | - | - | Leopard-facing Xcode project generation has been verified on Sierra with Xcode 9.2; generated projects have been copied back to Snow Leopard and build-verified there. |
| High Sierra 10.13 | :white_check_mark: | :white_check_mark: | - | - | - | Depends on the selected Xcode version; Xcode 9.4.1-era setups are expected to work, while Xcode 10.1 fails legacy UB1 generation. |
| Big Sur 11 through Monterey 12 | :white_check_mark: | :white_check_mark: | - | - | - | Default project generation can produce modern projects and Xcode 3.2-compatible bridge projects. |
| Monterey 12 / Xcode 14.2 / Snow Leopard Xcode 3.2.6 | :white_check_mark: | - | :white_check_mark: | - | - | After removing unsupported flags such as `-fno-objc-arc`, Monterey-generated bridge output has been four-architecture `lipo`-verified and runtime-verified on an iBook. |
| Ventura 13 / Xcode 15.2 and newer | :x: | :white_check_mark: | - | - | - | Modern native debugging works, but Xcode 15.2 only offered Xcode 12.0+ project formats in testing. |

## OS Workflow Notes

There are two separate legacy limits to keep in mind. Building UB1 outputs
directly requires a host that can launch Xcode 3.2.6; this has been verified
through Mavericks 10.9 and is not available on Yosemite. Generating projects for
that Xcode 3.2.6 workflow requires a newer-but-not-too-new Xcode generator host;
use hosts that can run Xcode 9.4.1 or earlier for legacy Leopard/Snow Leopard
bridge projects.

Snow Leopard is the main Xcode 3.2.6 bridge. Projects generated on tested newer
systems (10.8-10.13, and newer generator hosts) can be copied with the
repository tree and opened directly in Xcode 3.2.6. Generated Xcode projects are
absolute-path-sensitive: keep the same user/path layout used during generation,
or Xcode file references may break. With a copied project, Snow Leopard does
not need CMake or Ninja installed. Direct script builds such as
`build-10_4.sh` and `build-10_5.sh` do require CMake/Ninja on Snow Leopard and
are useful for standalone command-line build verification, but they do not
provide Xcode debugging.

Xcode 3.2.6 can build 32/64-bit Leopard-style UB1 outputs (`ppc7400`, `ppc64`,
`i386`, `x86_64`) and Tiger-oriented 32-bit UB1 outputs. For Tiger, set the
deployment target to Mac OS X 10.4 and use 32-bit Intel plus PPC; verified
outputs use `ppc i386` slices. Tiger runtime compatibility does not imply
Tiger/Xcode 2.4 project debugging: the Cocoa backend uses Objective-C
`@property` / `@synthesize` in a few internal delegate classes, so use Snow
Leopard/Xcode 3.2.6 for debugging Tiger-targeted builds.

Lion through Mavericks can also run a correctly installed Xcode 3.2.6 from a
Snow Leopard partition for Xcode-hosted UB1 work. With that setup,
Tiger-oriented 32-bit UB1 builds using GCC 4.0 have been build-verified from
the Xcode UI. The command-line wrappers are a secondary path there: they prefer
tools resolved through `xcrun` / the selected developer directory before
falling back to `PATH`, but Xcode 3.2.6 is an old `/Developer`-style install
rather than a modern `Xcode.app/Contents/Developer` bundle and cannot be
selected with `xcode-select` from those hosts. Direct Xcode UI verification is
therefore the supported route for that setup.

Leopard can open copied projects when a newer Xcode has saved them with Xcode
3.1 compatibility. If the copied project reports the original Base SDK as
missing, select the current Mac OS X 10.5 SDK in Xcode and build again. This has
been build-verified on both Intel and G4 Leopard systems. Do not expect the
`build-10_4.sh` / `build-10_5.sh` command-line scripts to work on Leopard:
CMake could not be installed via MacPorts in testing there, so those scripts
should be treated as Snow Leopard CLI verification paths.

Mountain Lion through Monterey can generate Xcode projects with
`gen-xcodeproj.sh`, so native debugging is available on the generating machine
with that host's final supported Xcode. The default path is 64-bit-only for
older Intel Macs, and Big Sur and later can use the UB2 path for
`arm64;x86_64` builds. Projects generated in that range can be prepared for
Xcode 3.2.6, and if the project is switched to Xcode 3.1 compatibility and
saved, the same project can be copied to Leopard for Xcode-hosted debugging
there. This is a bridge-project workflow through the default `gen-xcodeproj.sh`
path, not evidence that Monterey can directly generate a Leopard-deployment
project with the legacy UB1 generator.

On newer generator hosts, the deployment target used while generating an Xcode
project may need to be newer than the final target built on Snow Leopard. For
example, Monterey/Xcode 14 can run the default `gen-xcodeproj.sh` path and
generate a usable bridge project, then Snow Leopard/Xcode 3.2.6 can open that
project and perform the final UB1 build after removing unsupported flags such as
`-fno-objc-arc` and selecting the intended Base SDK, deployment target, and
architectures. That workflow has produced four-architecture UB1 output verified
with `lipo` and launched on an iBook.

For project-format down-conversion, Xcode 14.2 on Monterey has been observed to
offer Xcode 3.1 compatibility for a new project. Xcode 15.2 on Ventura only
offered Xcode 12.0 and newer formats in testing, so use an older generator host
when preparing projects for Leopard/Xcode 3.1.

High Sierra needs extra care because the selected Xcode version changes the
legacy generator behavior. Sierra with Xcode 9.2 has been checked and can
generate Leopard-facing Xcode projects. High Sierra with an Xcode 9.4.1-era
setup is expected to behave like earlier verified hosts. High Sierra with Xcode
10.1 and the MacOSX10.14.sdk has been checked and fails the Leopard and Lion UB1
generation paths during CMake's initial C++ compiler check because the toolchain
still selects `libstdc++` for the old deployment target, but the library is no
longer available. Treat Xcode 10 and newer primarily as modern macOS generator
hosts: Xcode 10/11 are useful for modern Intel-oriented projects, while UB2
(`arm64;x86_64`) starts with the Apple Silicon-capable Xcode generation.

## Which Script To Run

- `scripts/macos/build-10_4.sh`
  - Legacy build path for Tiger/Leopard compatibility work.
  - Defaults: `DEPLOYMENT_TARGET=10.4`, `ARCHS=ppc;i386`, `MAC_OS_10_4=1`
  - Intended for Snow Leopard environments with CMake/Ninja and the 10.4u SDK installed.
  - Build-verified on Snow Leopard with Xcode 3.2.6; expected merged slices are `ppc i386`.
  - Xcode UI builds using a Snow Leopard-partition Xcode 3.2.6 install have also been build-verified from Lion/Mountain Lion hosts.
  - Lion/Mountain Lion CLI builds through that Xcode 3.2.6 install are not supported because `xcode-select` cannot select it there.
  - `MAC_OS_10_4_SYSROOT` is auto-resolved from `DEVELOPER_DIR` / `xcode-select` when unset.
  - Prefers `gcc-4.0` / `g++-4.0` resolved through `xcrun` from the selected Xcode, then falls back to `gcc-4.2` / `g++-4.2` and `PATH`.
  - `ppc` builds require a working PPC-capable compiler/toolchain (validated by a compile check).
  - If PPC tools are missing, the build fails by default; set `ALLOW_PPC_FALLBACK=1` to continue with non-ppc archs.
  - Outputs are written under `build/macos-10.4-ub1`.

- `scripts/macos/build-10_5.sh`
  - Leopard multi-arch path excluding `ppc64`.
  - Defaults: `DEPLOYMENT_TARGET=10.5`, `ARCHS=ppc;i386;x86_64`, `MAC_OS_10_4=0`
  - `OSX_SYSROOT` is auto-resolved from `DEVELOPER_DIR` / `xcode-select` when unset, then falls back to `/Developer/SDKs/MacOSX10.5.sdk`.
  - Prefers `gcc-4.2` / `g++-4.2` resolved through `xcrun` from the selected Xcode, then falls back to `PATH`.
  - Build-verified on Snow Leopard with Xcode 3.2.6; expected merged slices are `ppc7400 i386 x86_64`.
  - Treat this as a Snow Leopard CLI verification path, not a Leopard-hosted script path.
  - By default builds app targets only (`LokaFloppyBirdMacOS`, `LokaHelloMacOS`, `LokaMineMacOS`, `LokaSimpleViewerMacOS`, `LokaTutorialMacOS`) and creates merged outputs in `build/macos-10.5-ub1/universal`.

- `scripts/macos/build-10_7.sh`
  - Standard build path for Lion and newer.
  - Defaults: `DEPLOYMENT_TARGET=10.7`, `MAC_OS_10_4=0`
  - On Snow Leopard with Xcode 3.2.6, CMake may warn that the `MacOSX10.6.sdk/Library/Frameworks` path is not set up correctly. The x86_64-only build has still been build-verified in that environment.
  - Treat this as a direct CLI build path for Intel macOS hosts whose installed Xcode still supports the requested deployment target. It is not the preferred path for preparing old Xcode bridge projects.

- `scripts/macos/build-ub2.sh`
  - Universal Binary 2 path for modern macOS.
  - Defaults: `DEPLOYMENT_TARGET=11.0`, `ARCHS=arm64;x86_64`, `MAC_OS_10_4=0`
  - By default builds app targets only (`LokaFloppyBirdMacOS`, `LokaHelloMacOS`, `LokaMineMacOS`, `LokaSimpleViewerMacOS`, `LokaTutorialMacOS`).
  - Treat this as a direct CLI build path for the UB2 generation, not as a fallback for legacy UB1 project generation.

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
  - In Xcode > Settings > Locations, set Command Line Tools to the Xcode version you want to use for generation. Equivalently, run `sudo xcode-select -s /Applications/Xcode.app/Contents/Developer`.
  - Requires CMake with Xcode generator support. Ninja is not required for Xcode project generation.
  - Intended for macOS Big Sur (11.0) and later by default.
  - For OS X Mountain Lion (10.8) and later, override `ARCHS` and `DEPLOYMENT_TARGET` explicitly.
  - Projects generated on 10.8+ are generally Xcode 3.2-compatible enough to be opened on Snow Leopard with Xcode 3.2.6.
  - Some newer flags may still need manual cleanup for Xcode 3.2.6, for example removing unsupported options such as `-fno-objc-arc`.
  - The generated project remains path-sensitive: source references are anchored to the path used when `gen-xcodeproj.sh` ran.
  - In practice, older Xcode versions may fail to resolve sources if the repository root folder name changes or if the project is reopened from a different absolute path after generation.
  - Treat generated `build/macos-xcodeproj/...` projects as machine-local artifacts; regenerate them on the target machine/path instead of copying them as reusable nightly-build artifacts.
  - Legacy `MAC_OS_10_4*` flags are intentionally not used here.

- `scripts/macos/gen-xcodeproj-leopard-ub1.sh`
  - Experimental Leopard/Snow Leopard Universal Binary 1 Xcode project generation path.
  - Defaults: `OSX_SYSROOT=macosx`, `DEPLOYMENT_TARGET=10.5`, `ARCHS=xcode-standard-32-64`.
  - Generates under `build/macos-xcodeproj-leopard-ub1/<build-type>-<arch-mode>`.
  - Requires a CMake/Xcode generator environment with Xcode 5.0 or newer; generate on a newer Mac and copy the project to Snow Leopard for Xcode 3.2.6 testing.
  - Make sure Xcode > Settings > Locations > Command Line Tools points at that full Xcode.app before running the script.
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
  - For Tiger 32-bit UB1 builds in Xcode 3.2.6, set the deployment target to Mac OS X 10.4, use only 32-bit architectures, and choose 32-bit Intel plus manually added `ppc` when using the Xcode architecture picker. Verify the final slices with `lipo`; expected slices are `ppc i386`.
  - This path is experimental but has been build-verified after copying generated projects to Snow Leopard/Xcode 3.2.6 and Leopard/Xcode 3.1-compatible setups.

- `scripts/macos/gen-xcodeproj-lion-ub1.sh`
  - Experimental Lion-era Universal Binary 1 Xcode project generation path.
  - Defaults: `OSX_SYSROOT=macosx`, `DEPLOYMENT_TARGET=10.7`, `ARCHS=xcode-standard-32-64`.
  - Generates under `build/macos-xcodeproj-lion-ub1/<build-type>-<arch-mode>`.
  - Requires a CMake/Xcode generator environment with Xcode 5.0 or newer; generate on a newer Mac and copy the project to Lion-era Xcode for testing if needed.
  - Make sure Xcode > Settings > Locations > Command Line Tools points at that full Xcode.app before running the script.
  - `ARCHS=xcode-standard-32-64` omits `CMAKE_OSX_ARCHITECTURES` and writes Xcode's `$(ARCHS_STANDARD_32_64_BIT)` preset into the generated project.
  - Keeps Loka's explicit `-fno-objc-arc` example-target flags enabled, unlike the Leopard/Xcode 3.2 compatibility path.
  - Sets `CLANG_ENABLE_OBJC_ARC=NO`, `ONLY_ACTIVE_ARCH=NO`, and suppresses CMake's regeneration target in the generated project.
  - The Snow Leopard/Xcode 3.2.6 manual steps are otherwise similar to the Leopard path: select the intended Base SDK and deployment target, then add/choose the needed PPC and Intel architectures in Xcode.

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

# Generate experimental Leopard/Snow Leopard UB1 Xcode project
./scripts/macos/gen-xcodeproj-leopard-ub1.sh

# Generate experimental Lion-era UB1 Xcode project
./scripts/macos/gen-xcodeproj-lion-ub1.sh
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

## Troubleshooting

If Xcode.app is installed but CMake reports
`No CMAKE_CXX_COMPILER could be found`, launch Xcode once, accept the license,
and install the required components. Then verify the selected developer tools:

```bash
xcode-select -p
xcodebuild -version
xcrun -find clang++
```

If needed, select the full Xcode app and complete first-launch setup:

```bash
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
sudo xcodebuild -license
sudo xcodebuild -runFirstLaunch
```

When keeping multiple Xcode.app versions side by side, avoid spaces in the app
bundle name used for legacy project generation. For example,
`/Applications/Xcode9.4.1.app/Contents/Developer` works more reliably than
`/Applications/Xcode 9.4.1.app/Contents/Developer`; the latter can make CMake's
Xcode generator fail to find `CMAKE_CXX_COMPILER` even when `xcode-select`,
`xcodebuild`, and `xcrun` appear to resolve the selected Xcode correctly. This
appears to be a generator/toolchain path-handling issue, not simply a wrapper
script argument-splitting problem.
