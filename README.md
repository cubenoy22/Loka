# Loka

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="assets/Hero-dark.svg">
  <source media="(prefers-color-scheme: light)" srcset="assets/Hero.svg">
  <img alt="Loka cover artwork" src="assets/Hero.svg">
</picture>

> [!IMPORTANT]
> This framework is still in the proof-of-concept stage. The core is already usable, but broader component coverage, platform support, and some refactoring work are still in progress. Please also see [ROADMAP.md](ROADMAP.md).

## Why Loka?

Loka aims to bring new content and creative tools to retro platforms without treating them as novelty targets.

It is not just for GUI applications. Loka is also intended to grow toward animation, video, and game production on G3-class systems and later, where timelines, sprites, models, and characters can be treated as nodes in the same declarative system.

Loka's core idea is to keep the cast of concepts small and meaningful. `Node`, `Boundary`, `State`, `Props`, `Flow`, and platform projection are reused consistently across UI, events, async pipelines, and native integration instead of introducing a new mechanism for every feature.

That consistency is intentional. A `Boundary` is also a kind of `Node`; `NodeState<T>` is the state handle a node uses while storage and tracking remain attached to a lifecycle owner; `FlowSlot<T>` gives long-lived flows the same kind of owner-aware lifetime. The goal is code that feels modern and declarative while still making ownership, cleanup, and update routing explicit.

## How does it work?

Loka uses a modern statically typed DSL built in C++98 to declaratively define UI and application structure, then projects it onto each target OS.

- One declarative model is shared across platforms.
- Application logic stays in portable C++98 code.
- The public API tries to avoid exposing manual memory management in ordinary app code.
- Strong types carry meaning: node-owned state, borrowed state, props input, flow lifetime, and platform projection are distinct.
- The core depends on only a small subset of the STL, helping it stay highly portable across old and new toolchains.
- Each target maps that structure onto native windowing and drawing APIs.
- The core stays neutral while platform layers stay thin.

---

## Bridging Modern Development and Retro Environments

Strong static typing, no exceptions, no RTTI, and only a small STL surface.

### Challenges

- CPU limits on 68k-era systems
- small memory budgets
- older compiler and toolchain constraints such as GCC 4.0-era environments
- explicit error handling and manual memory management
- a minimal dependency surface

### Approach

- a modern statically typed DSL built in C++98
- strong compile-time type safety despite a C++98 core
- no exceptions and no RTTI in core DSL paths
- declarative UI and application structure
- a small, unified concept set instead of many special-purpose mechanisms
- deterministic lifecycle management behind app-facing declarative APIs
- logical UI design separated from OS-specific projection
- portable application logic with thin platform layers
- reliance on only a small subset of the STL

For deeper design notes, see [docs/ProgrammingGuide.md](docs/ProgrammingGuide.md) and [docs/environments.md](docs/environments.md).

---

## Building

Loka uses a **CMake + Ninja** based build system.

### Prerequisites

For the main development and test workflow:

- CMake 3.19 or newer when using `CMakePresets.json`
- Ninja
- A C++ compiler capable of building C++98 code

The core is intentionally C++98-friendly and already builds with older toolchains such as GCC 4.0-era environments. Modern host builds can use current Clang, GCC, or MSVC.

Platform-specific builds also need the matching native toolchain:

- macOS: Xcode or Xcode Command Line Tools
- Windows: Visual Studio Build Tools or Visual Studio, usually from a matching Developer Command Prompt
- Classic Mac OS targets: Retro68, typically from a Linux, WSL, Docker, or container-based environment

For a quick headless test build on Linux/WSL:

```sh
cmake -S . -B build/Testing -DTEST_BUILD=ON
cmake --build build/Testing
ctest --test-dir build/Testing
```

Development, build, and target environment notes are documented in [docs/environments.md](docs/environments.md).

Classic Mac OS and Retro68-specific notes are documented in [docs/retro68.md](docs/retro68.md).

macOS script entry points are documented in [scripts/macos/README.md](scripts/macos/README.md).

---

## License

This repository is released under the **MIT License**.
