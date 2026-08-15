# iPhone OS 3.1 ARMv6 Smoke

This profile is intentionally separate from the current-Xcode iOS generator.
It builds the smallest typed UIKit host for a first-generation iPod touch
without teaching the modern target about obsolete lifecycle or toolchain
details.

## Preserved inputs

The golden build expects:

- an Intel Mac or VM with the preserved `/Developer` toolchain;
- Xcode 3.2.6 (build 10M2518);
- the iPhoneOS 4.3 device SDK installed under that toolchain;
- iPhone OS 3.1.3 DeviceSupport for physical-device symbols and debugging;
- GCC 4.2 from the iPhoneOS platform;
- a separate record of the installer source, original filename, license/EULA,
  acquisition date, SHA-256, host OS, and VM snapshot.

Do not commit an Apple SDK or installer to this repository.

The preserved installer observed on the verification host is:

- filename: `xcode_3.2.6_SnowLeopard.dmg`;
- SHA-256: `47f6fecdce63bb00640a9f99c2a65778c1356630286d8003d597ffb83dfcdf3b`;
- volume: `Xcode and iOS SDK`, read-only HFS+;
- included installer: `Xcode and iOS SDK.mpkg`;
- included reference: `About Xcode and iOS SDK.pdf`;
- hash verified: 2026-08-15.

The original download source, acquisition date, and EULA disposition remain
external provenance gaps. The installer's `iPhoneSDK3_1_3.pkg` payload is
DeviceSupport and device symbols, not a 3.1.3 header/link SDK. The golden build
therefore correctly uses the iPhoneOS 4.3 SDK while setting the deployment
target to 3.1.

## Build

Run on the preserved Mac:

```sh
./scripts/ios/legacy/build-armv6.sh
```

Override the toolchain or output location explicitly when needed:

```sh
LOKA_LEGACY_DEVELOPER_DIR=/Developer \
BUILD_DIR=build/ios-legacy-armv6 \
./scripts/ios/legacy/build-armv6.sh
```

The builder fixes the compatibility contract in one place:

- `armv6` only;
- iPhoneOS 4.3 base SDK with deployment target `3.1`;
- C++98 and the toolchain's `libstdc++`;
- Objective-C 1 style manual reference counting;
- exceptions and RTTI disabled;
- UIKit, Foundation, and CoreGraphics linked directly.

It builds in a temporary stage, verifies the Mach-O CPU subtype is `ARM V6`,
and commits only the inspected unsigned zip to the output directory. The app
uses the pre-UIScene application delegate and a direct UIWindow view tree;
that duplication from the modern smoke host is a deliberate platform seam.

## Verification state

The first command-line build was verified on 2026-08-15 using macOS 10.9.5,
Xcode 3.2.6, Apple GCC 4.2.1, and the iPhoneOS 4.3 SDK. Compilation passed with
`-Wall -Wextra -Werror`; the linked executable reported Mach-O `ARM / V6` and
the unsigned app bundle was 20 KiB before archival. The same installation has
DeviceSupport for 3.0, 3.1, 3.1.2, 3.1.3, 3.2.x, and 4.0 through 4.3.

Physical-device runtime verification was completed on 2026-08-16 using a
first-generation iPod touch (`iPod1,1`) running iPhone OS 3.1.3 (build 7E18).
The device was jailbroken without a restore by running redsn0w 0.9.3 on a
physical MacBook2,1 with Mac OS X 10.7.5. redsn0w used the verified iPod1,1
iPhone OS 3.1.2 (build 7D11) IPSW as its compatibility input; the installed OS
remained 3.1.3.

The bundle was ad-hoc signed on macOS 10.9.5 with `codesign --sign -`. The
result retained its Mach-O `ARM / V6` subtype and used an embedded SHA-1
`CodeDirectory` version 20100. A normal `installation_proxy` deployment reached
`VerifyingApplication` and returned `ApplicationVerificationFailed`, confirming
that jailbreak state alone did not make the provisioning-based IPA path valid.
The successful route copied the signed bundle over USB-tunneled OpenSSH to
`/Applications`, set the executable mode to 0755, and restarted SpringBoard.

SpringBoard then enumerated the verification artifact under bundle identifier
`com.cubenoy22.loka.legacy-hello-world`, version 0.0.3, with display name
`Loka 3.1.3 Hello`. Touch launch and the full-screen white UIKit view containing
`Hello, iPhone OS 3.1.3!` were visually confirmed on the physical device while
USB syslog capture was active. This profile is therefore both `build-verified`
and `runtime-verified`; no Simulator result is used as ARMv6 runtime evidence.

The checked-in Info.plist now uses
`io.lo-ka.loka.legacy-hello-world`, following the project's `lo-ka.io` bundle
namespace decision after that run. The executable and UIKit view source are
unchanged, but deployment under the renamed identifier has not yet been
`runtime-verified` on the device.
