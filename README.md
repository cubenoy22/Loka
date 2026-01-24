# Loka

> A declarative UI & application framework that runs from **Classic Mac OS (68k, System 7)** to **modern macOS, Windows, Linux**, and beyond.

Loka is an experimental but *real* declarative framework written in **strict C++98**, designed to prove a bold idea:

> **One reactive UI model can scale from a 68000 @ 8 MHz Macintosh to modern multi‑core systems.**

This is not a toy, not a transpiler, and not a JS runtime.
Loka produces **native applications**, with direct access to each platform’s windowing and graphics APIs.

> **Loka enables declarative UI applications to be written in a single C++98-based syntax across platforms ranging from Classic Macintosh systems to modern operating systems, including PDA-class devices.**

---

## Why Loka exists

Modern declarative UI frameworks (React, SwiftUI, Compose, etc.) assume:

* abundant memory
* fast CPUs
* garbage collection or heavy runtimes
* short software lifetimes

Loka assumes the opposite.

It is designed around constraints that *cannot* be ignored:

* Motorola 68000 @ 8 MHz
* hundreds of kilobytes, not gigabytes
* deterministic execution
* explicit ownership and lifetimes

If it works there, it works anywhere.

---

## Key properties

* **Declarative UI** (React / Solid‑like mental model)
* **Fully reactive** state propagation
* **Single‑source‑of‑truth (MISO‑oriented)** design
* **No garbage collector**
* **No exceptions**
* **C++98 only** (by design)
* **Native backends** (Toolbox, Win32, Cocoa, etc.)

Loka favors *locality*, *predictability*, and *auditability* over convenience.

---

## Supported platforms (current & planned)

| Platform                             | Status                                      |
| ------------------------------------ | ------------------------------------------- |
| Classic Mac OS (68k / PPC)           | ✅ working                                   |
| Mac OS X Tiger / Leopard             | ✅ working                                   |
| Modern macOS (Intel / Apple Silicon) | ✅ working                                   |
| Windows (Win32)                      | ✅ Windows XP verified (Windows 10 Mobile is next target) |
| Linux                                | 🟡 community-driven (future)                |
| iOS / iPadOS                         | 🟡 expected to be possible                  |
| Windows Mobile / PocketPC            | 🟡 community-driven (future)                |

**Minimum RAM (Classic Mac OS examples):** ~300 KB or above

**Disk size (Classic Mac OS examples):** fits on a **2DD floppy disk (800 KB)** or larger media

> Current example binaries are up to **~271 KB** in size.
> Figures reflect the current set of example applications targeting System 7.
> Actual requirements depend on UI complexity and platform.

> If it runs on System 7, it can probably run on your device.

---

## Packages

Loka is structured as a small set of clearly separated packages.
This keeps applications explicit about what they depend on, and allows the framework to scale from tiny System 7 examples to more complex applications.

### `loka::core`

The **mandatory core package**.

* Required for *all* Loka-based applications
* Provides the declarative UI model, reactive state system, and core abstractions
* Platform-agnostic and freestanding

If you are building an app with Loka, you always start here.

---

### `loka::app`

The **application-layer package**.

* Required for OS-level applications
* Provides windowing, event loops, and platform integration
* Contains per-OS implementations (Classic Mac OS, Win32, macOS, etc.)

Most end-user applications depend on `loka::core` + `loka::app`.

---

### `loka::game` (planned)

A **high-level declarative game framework** built on top of Loka.

* Game loop abstractions (GLUT-like foundations)
* Declarative scene and entity construction
* Designed for *high-level* game creation, not low-level engine work
* Planned support for **3D rendering pipelines oriented toward video capture and offline rendering**

The goal is to make it possible to write portable, declarative games using the same mental model as Loka UI, while remaining lightweight enough for constrained systems.

---

## Philosophy

Loka is built on a few strong beliefs:

* **State should be explicit.**
* **Updates should be traceable.**
* **UI trees should be cheap to rebuild.**
* **APIs should fail at compile time when possible.**
* **A framework should not outgrow its applications.**

This project intentionally avoids trends that conflict with long‑term maintainability.

---

## Status

This repository is:

* **usable**
* **highly experimental**
* **incomplete**
* **actively evolving**

Some areas are intentionally rough.
Some APIs *will* change.

While Loka can already build real applications, it is **not yet optimized for large-scale production app development**.
At the moment, app projects may require **significant boilerplate** (platform glue, build wiring, and hand-written integration code).

The goal of this release is **demonstration and exploration**, not stability guarantees.

---

## Who this is for

* engineers interested in **declarative UI internals**
* retro‑computing enthusiasts
* people curious about extreme portability
* anyone who feels modern frameworks are too opaque

This is *not* aimed at beginners, and it is **not a drop-in production framework** (yet).

---

## Building

Loka uses a **CMake + Ninja** based build system.

### Native development (macOS / Windows)

On modern macOS or Windows, you can develop and build Loka natively by installing:

* VS Code
* CMake
* Ninja
* **Xcode Command Line Tools** (macOS)
* **Visual Studio Build Tools** (Windows)

  * **VS 2017** required for Windows XP (static linking with legacy VC++ runtime)
  * Newer toolchains (e.g. VS 2026) may not support XP-compatible static builds
* a C++98-capable compiler (Clang or MSVC)

This setup allows fully native development.

> **Note (Windows / MSVC):**
> When developing on Windows, VS Code should be launched from a **Visual Studio Developer Command Prompt** (or equivalent MSVC-initialized shell) so that the MSVC environment variables are correctly set.
>
> **Windows XP builds require extra care:**
>
> * Install **Visual Studio 2017**
> * Use **x64_x86 Cross Tools Command Prompt for VS 2017**
> * Launch VS Code from that prompt using `code .`
>
> Newer Visual Studio prompts or default shells may select incompatible runtimes.
>
> On **Windows on ARM**, builds currently target **ARM only**.
> On macOS, **Parallels Desktop** can be used to host a Windows environment, enabling a *single-machine* workflow for both platforms.

> **Note (macOS targets):**
> Until UB1 support is available, the minimum macOS deployment target may vary depending on the Xcode + CMake environment.
> In practice, targets may range from **OS X Lion (Objective-C 2.0 with ARC)** up to **macOS 11 Big Sur or later**.

### Classic Mac OS (68k / PPC via Retro68)

For Classic Mac OS targets, Loka is built using **Retro68**.

Recommended environments:

* Linux
* WSL (Windows Subsystem for Linux)
* macOS with Linux containers (e.g. colima)

The typical workflow is:

1. Build the application using Retro68 in a Linux environment
2. Copy the resulting binary to:

   * Mini vMac, or
   * a real Classic Mac OS system (System 7–Mac OS 9)
3. Verify behavior on real or emulated hardware (currently observed primarily on Mini vMac at 1× speed)

This separation keeps the modern toolchain clean while preserving accurate classic‑environment testing.

### Toolchains & binary formats

* **Universal Binary 2 (UB2)**: ✅ ready
* **Universal Binary 1 (UB1)**: 🟡 planned (Xcode 3.2.6)
* **Classic builds**: 🟡 planned (CodeWarrior Pro R5)
* **Windows XP**: ✅ verified (requires **Visual Studio 2017** toolchain)
* **Windows legacy toolchains**: 🟡 planned (Visual Studio 2005)

> Note: Newer MSVC toolchains may link against modern VC++ runtimes that cannot be statically built for Windows XP.

These targets reflect the author’s long-term goal of keeping Loka buildable with historically accurate toolchains where possible.

> Build instructions will continue to evolve as the project stabilizes.

---

## License

This repository is released under the **MIT License**.

---

## Name

"Loka" is the project name.

The name "Declara!" refers to an earlier internal concept and is not part of the public API or branding at this time.

---

## Disclaimer

This project exists because the author *wanted to know if this was possible*.

Turns out: it is.

Whether it is *useful* is up to you.
