# iOS Development Smoke

This directory owns the disposable modern iOS Xcode-project generation path.
It proves that the selected Apple toolchain can build and launch a small UIKit
application; it is not the Loka iOS platform projection.

## Modern iPhone and iPad

Requirements:

- macOS with a full Xcode installation selected by `xcode-select`;
- CMake with the Xcode generator;
- an installed iOS Simulator runtime, or an attached device and a signing team.

Generate the project without pinning an architecture:

```sh
./scripts/ios/gen-xcodeproj.sh
```

CMake selects the device SDK by default. Generate a separate disposable
project for Simulator when no signed physical device is involved:

```sh
IOS_SYSROOT=iphonesimulator \
BUILD_DIR=build/ios-xcodeproj/Debug-simulator \
./scripts/ios/gen-xcodeproj.sh
```

The generated project is under `build/ios-xcodeproj/Debug` by default. Open
`Loka.xcodeproj`, select the `LokaUIKitHelloWorld` scheme, and run it on an
iPhone or iPad destination. The target declares device families `1,2`; Xcode
owns the selected architecture, bundle signing, installation, and launch.
Both generated projects contain the same `LokaUIKitHelloWorld` target and
source; the sysroot selects the native execution environment without inventing
a second application target. The modern smoke target uses the `UIScene`
lifecycle and requires iOS 13 or newer.

Set `DEPLOYMENT_TARGET` only when a specific compatibility floor is being
tested:

```sh
DEPLOYMENT_TARGET=15.0 ./scripts/ios/gen-xcodeproj.sh
```

The generated project is disposable. Keep persistent settings in CMake or a
future checked-in configuration file rather than editing the project by hand.

## First-generation iPod touch

Treat ARMv6 / iPhone OS 3.1.3 as a separate legacy Apple profile. It requires a
preserved iPhone SDK and an old toolchain that can emit ARMv6 code; the modern
generator above does not claim to provide either. The checked-in
`legacy/build-armv6.sh` profile uses Xcode 3.2.6, its iPhoneOS 4.3 SDK, GCC 4.2,
and deployment target 3.1. Its [legacy README](legacy/README.md) records the
verified toolchain facts and the remaining provenance and runtime gaps. Do not
commit an SDK, installer, or device firmware.

The direct UIKit application is `build-verified` as ARMv6 with C++98 /
Objective-C 1 manual reference counting, exceptions disabled, and RTTI
disabled. A launch on the physical iPod touch remains the separate
`runtime-verified` acceptance point.
