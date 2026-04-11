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

## How does it work?

Loka uses a modern statically typed DSL built in C++98 to declaratively define UI and application structure, then projects it onto each target OS.

- One declarative model is shared across platforms.
- Application logic stays in portable C++98 code.
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
- logical UI design separated from OS-specific projection
- portable application logic with thin platform layers
- reliance on only a small subset of the STL

For deeper design notes, see [docs/ProgrammingGuide.md](docs/ProgrammingGuide.md) and [docs/environments.md](docs/environments.md).

---

## Building

Loka uses a **CMake + Ninja** based build system.

Development, build, and target environment notes are documented in [docs/environments.md](docs/environments.md).

Classic Mac OS and Retro68-specific notes are documented in [docs/retro68.md](docs/retro68.md).

macOS script entry points are documented in [scripts/macos/README.md](scripts/macos/README.md).

---

## License

This repository is released under the **MIT License**.
