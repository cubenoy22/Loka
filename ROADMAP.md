# Loka Roadmap

This document outlines the planned milestones for Loka.
Dates are intentionally omitted; progress is guided by stability, portability,
and validation on real hardware rather than a fixed calendar.

Versioning follows a pragmatic pre-1.0 scheme:

* `0.0.x` — rapid iteration and structural cleanup
* `0.1.x` — `loka::core` stabilizes
* `0.2.x` — `loka::app` stabilizes
* `0.3.x` — platform and ecosystem expansion

---

## v0.0.1 (current)

Initial public milestone.

Focus:

* Core architectural ideas validated in real builds
* Examples running on at least one modern platform
* Project rules and constraints made explicit (C++98-friendly, no exceptions, etc.)

---

## v0.0.x — Bugfixes & Toolchain Support

### Goals

* Reduce friction during rapid iteration
* Improve correctness and portability
* Make builds reproducible and predictable

### Planned

* Bugfixes & refactors

  * Memory safety improvements and leak fixes
  * Rendering correctness and layout fixes
  * Performance tuning where it matters (layout, diffing, redraw)
* Documentation cleanup

  * Consolidate DSL and API quick-start docs
  * Expand examples with minimal, focused walkthroughs
* Toolchain compatibility

  * **Xcode 3.2.6 UB1 support**

    * ARC disabled
    * objc1.0 constraints respected where applicable
  * Compatibility with older clang/gcc toolchains
* Build hygiene

  * Clearer build instructions
  * Reduced platform-specific hacks
  * Prepare for CI introduction in 0.1.0

---

## v0.1.0 — `loka::core` API Stability

### Goals

* Stabilize the `loka::core` API
* Enable long-lived applications without frequent breakage
* Establish Loka as a reusable foundation rather than a moving target
* Finish the first "structural completeness" pass before broad platform/control expansion
* Remain explicitly Proof of Concept through `v0.1.x`

### Design gate for v0.1.0

Before pushing hard on new controls or new platforms such as iOS/Linux, the
project should aim to make the current architectural seams feel complete enough
that feature work can mostly become additive.

In practice, that means `v0.1.0` should ideally reach the point where:

* New node/control behavior does not require `PlatformController` growth by default.
* New retained layout/container behavior can follow shared layout helpers and internal layout handler seams rather than ad-hoc platform branches.
* Layout helper contracts are polished enough that container layout work is predictable: return values, child traversal callbacks, sizing heuristics, and platform projection responsibilities are named and tested explicitly.
* `Boundary` responsibilities are understood well enough that future Menu/Window DSL work can reuse the same kernel ideas instead of inventing separate update/apply models.
* A more complete real application has exercised the app-wide ownership model enough to design Repository/ApplicationScope-style access deliberately, without turning `App` itself into a `BoundaryNode`.
* App/window platform conventions, such as application-wide menus on macOS/Classic Mac and window-attached menus on Win32, are captured as explicit policy instead of implicit platform code.
* CMake targets reflect the major module boundaries instead of each platform target compiling all of `common/*.cpp` directly. Start with `LokaCommon`, then split toward `LokaCore`, `LokaDsl`, `LokaApp`, and future optional modules as dependencies become clear.
* The current Win32/macOS internal seams are documented and tested well enough that Toolbox can follow later without changing the public controller contract.
* Contribution paths are visible: examples, docs, and tests should show where custom nodes, handlers, and future layout extensions belong.

The intent is:

* `0.0.x` through `0.1.0`: finish structural cleanup and seam definition
* after `0.1.0`: bias toward additive work, contribution friendliness, and API/documentation hardening up to `1.0.0`

Practical interpretation:

* until `v0.1.x`, Loka should prioritize proving that its core architectural model is coherent across current targets over maximizing platform count or control count
* new work before `v0.1.0` should primarily strengthen the model, seams, and examples rather than widen the surface area prematurely
* near-term Classic performance work should focus on proving that modern Loka
  concepts are practical on PPC601-era and newer machines; avoid deep
  68k/68030-first micro-optimization unless profiling shows a specific
  regression or correctness problem
* Repository/ApplicationScope design should be driven by a real application built after `v0.0.1`, so app-owned services, window-owned state, and scene `Boundary` access are separated by actual usage pressure
* after `v0.0.1`, scale up through real application work toward the long-term app-layer proof: Loka should be able to support professional applications that are awkward or fragile to build on existing frameworks, while keeping the application's core meaning portable and inspectable

### Planned

* **`loka::core` API stability**

  * Core concepts and public APIs frozen
  * Breaking changes strongly discouraged after this point
  * Documentation upgraded from exploratory to reference-level
* **Toolbox smart redrawing (planned)**

  * Smarter invalidation / dirty-rect handling
  * Reduced redundant redraws
  * Better responsiveness on slow CPUs/GPUs
  * Optimize first for measured PPC-era usability and redundant redraw
    suppression; keep 68k-friendly constraints in view, but defer extreme
    68k-only specialization to a separate evidence-driven track.
* **Core components**

  * `LazyList` (planned)
  * Additional layout primitives and common widgets
* **Layout architecture polish**

  * Clarify the shared container layout contract before `v0.1.0`, especially
    whether a helper returns the next vertical position, the occupied lower
    bound, or a maximum child lower bound.
  * Name the child-layout callback shape explicitly instead of repeating
    `void *context` plus function-pointer signatures in each helper.
  * Keep `Box`/`Column`/`Row`/`Grid`/`ZStack` algorithms separate while their
    behavior differs, but remove accidental inconsistency in naming, guards,
    metrics, and tests.
  * Make the platform layout handler seam clear enough that future custom
    containers and Toolbox parity can be added without growing the platform
    controller switch paths.
* **Early authoring and animation surface**

  * Add a small first-pass animation/property surface that can express simple
    movement, timing, and state-driven transitions without committing to the
    final multimedia/game API shape.
  * Keep animation intent explicit about where work is projected, such as
    platform/CPU drawing versus future composition/GPU-backed paths, so the
    application remains understandable instead of relying on hidden runtime
    policy.
  * Validate that Loka can support lightweight HyperCard-style software:
    card/stack-like screens, simple interactions, stateful widgets, scripted or
    Flow-driven actions, and portable authoring patterns that can later scale
    toward games, video tools, and professional applications.
* **Project infrastructure (minimum)**

  * Transfer repository to the **Loka GitHub organization**
  * Introduce baseline CI

    * Build checks for at least one Unix-like platform
    * Additional platforms added incrementally
  * Add `CONTRIBUTING.md`

    * Build instructions
    * Contribution guidelines
    * Lightweight PR expectations
  * Enable Discussions for design and usage questions

---

## v0.2.0 — `loka::app` API Stability & Game Experiments

### Goals

* Make the application layer stable and predictable
* Support real desktop-style applications
* Begin structured experimentation for game development

### Planned

* **`loka::app` API stability**

  * Application and window lifecycle finalized
  * Resource and platform integration clarified
* **Game development (experimental, planned)**

  * Optional modules rather than core bloat
  * Early experiments:

    * Scene-style structures
    * Timing / loop abstractions
    * Input patterns usable on both retro and modern platforms
  * No stability guarantees yet (explicitly experimental)

---

## v0.3.0 — Platforms & Ecosystem Expansion

### Goals

* Expand supported platforms without compromising simplicity
* Strengthen the surrounding ecosystem

### Candidates

* Additional desktop platforms (Windows / Linux variants)
* Select legacy targets where feasible
* Early mobile exploration (only if architecture remains clean)
* Improved samples, documentation, and tooling
* Optional packaging / distribution helpers
* Possible ultra-lightweight experiments such as a `LokaLite`-style profile for
  stricter 68k/low-memory targets, kept separate from the mainline proof that
  modern Loka concepts work well on PPC-era and newer systems

---

## Guiding Principles

* Stability is a feature
* Portability is intentional, not accidental
* Core remains lean; experiments live at the edges
* Real hardware validation beats theoretical completeness
