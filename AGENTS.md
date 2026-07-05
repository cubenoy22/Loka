# Project rules

For proposal and design judgment, also follow [PHILOSOPHY.md](PHILOSOPHY.md).
When choosing between plausible implementations, prefer the option that best
matches Loka's philosophy of meaningful app-facing code, explicit ownership,
clear boundaries, and small reusable concepts.

## Core Constraints
- Loka repository code should remain compatible with the project's target platform constraints; treat C++98 as the baseline unless a narrower file- or platform-specific rule explicitly allows otherwise.
- Prefer compile-time errors over runtime checks; leverage templates, inheritance constraints, and SFINAE to catch misuse at build time.
- Use TypeTag static checks automatically when the compiler supports `static_assert` (C++11+). In C++98 builds, keep optional TypeTag checks behind explicit `LOKA_*_CHECK_TYPETAG` gates so classic/release builds stay lightweight.
- C++ exceptions are disabled; do not add `try`/`catch` or rely on throwing.
- Loka applications are single-threaded at the application layer; all UI and DSL logic must execute on the Main Thread. Multi-threading and concurrency are deferred to OS/platform-specific implementations, which must dispatch results back to the Main Thread.
- Prefer explicit error handling and nothrow/nullable patterns in Classic builds.
- Use `assert` for contract violations (e.g., null PlatformContext); do not throw.
- Secrets/PII must not be hardcoded; use env vars and avoid logging sensitive data.
- Use English for code comments, code-facing docs, and API/design notes that ship with the repository; keep non-English prose for user conversation only unless a file already has an established localized convention.

## Ownership And State
- Keep scopes small by default. Prefer immutable completed values, explicit owners for mutation, and small encapsulated types when a feature needs multiple pieces of internal state.
- Before adding new variables, especially member fields, consider whether they introduce long-term ownership/lifecycle/cleanup complexity. Prefer reusing an existing owner or encapsulating the state so management does not become more fragmented over time.
- When state or variables must be introduced, consider whether they should be encapsulated or expressed as a small state machine instead of scattered flags. Prefer lifecycle-aware structures that make ownership, transitions, and cleanup easier to reason about.
- Prefer immutable value objects for facts, snapshots, props, plans, and completed analysis results. Use builders, local temporaries, or explicit owner state for construction/mutation, then expose the completed value through a read-only API. If this choice is ambiguous before implementation, stop and confirm whether the feature should prioritize immutable structure or measured mutation/reuse.
- Separate facts from operations. A growing set of mutable properties plus general-purpose methods usually means a class is becoming an accidental owner/controller. Prefer small value objects, stack-local diff/result construction, Flow/action/policy objects, repositories, or platform seams over turning any long-lived framework object into a broad controller.
- Performance-sensitive mutation is allowed when 68030-era or other supported target constraints make copying too costly, but the mutable phase, owner, and cleanup path must remain explicit. Prefer splitting immutable sections into separate value types over making a broad object mutable.
- MutableState<T>::set() must be wrapped in a StateTracker transaction (use RAII guard).
- Ownership policy: follow gravity. Parent owns child-facing state/data by default; cross-boundary sharing must be explicit and should use a meaningful owner/facade; `Managed<T>` may stabilize payload lifetime but should not replace a real state/resource owner. Broad reuse should prefer global caches for immutable/shared resources. Avoid designs where a child effectively owns or stabilizes its parent.
- Boundary access policy: `currentBoundary()` is the owner-side path; `findBoundary()` is for direct-parent borrowed access only. Do not rely on multi-hop or sibling boundary traversal from DSL code.
- State creation policy: prefer lifecycle-aware APIs such as `state()` and `declareStates()` for ordinary Node/Boundary-owned mutable state. Treat ad hoc state creation helpers such as `dangerouslyUseState()` / `dangerouslyUseManagedState()` as escape hatches that require explicit review.
- Dangerous state API policy: keep `dangerously*` state access/creation callsites out of normal `common/` and `example/` DSL code unless there is a documented reason. A new `dangerously*` usage should be treated as a design event, not routine implementation.
- NodeState storage policy: a component may keep `NodeState<T>` members only for its own Node/Boundary-owned state declared through lifecycle-aware state APIs such as `state()` or `declareStates()`. Do not expose `NodeState<T>` across boundary lines or use it as a foreign mutation channel.
- NodeState pass-through policy: when a DSL/API needs read-only live state, pass `nodeState.state()` explicitly instead of relying on implicit conversions from `NodeState<T>`.
- NodeState internal-surface policy: owner/tracker access on `NodeState<T>` is internal and should stay behind `dangerously*` naming; ordinary DSL code should use `get()`, `set()`, and `.state()` only.
- Flow ownership policy: long-lived Flow/StateStream chains owned by a Node or Boundary should be stored in `FlowSlot<T>` or an equivalent lifecycle-aware slot. Keep one-shot stack Flow usage limited to tests or bounded local operations.
- Flow state policy: do not share mutable state between unrelated Flow instances just because a value must change. Prefer Flow-owned input/result state, dedicated result state, or read-only input state. When multiple inputs should drive one event/result, consider `DerivedState`, an emitter adapter, or an explicit owner facade rather than passing the same `MutableState<T>*` through several Flow paths.
- Ownership/binding policy: distinguish borrowed live state from props-owned constant values explicitly. Props-owned constant values may reuse internal storage helpers, but they must not be registered as observed state or bound/unbound through NativeContext live-state paths.

## Design Taste
- Prefer designs that make ownership, lifecycle, and update flow visible in the type or API shape.
- Prefer code that can be reviewed safely by shape. During detailed design, treat it as a high-priority concern that safety, relationships between objects, ownership, lifecycle, mutability versus one-shot construction, mutation points, and projection boundaries are visible from names, types, and structure without requiring every line to be mentally executed. Memory ownership should be visibly under a Boundary/owner scope or an explicit higher-level owner, and State updates should not create pinball-like chains unless Flow, interaction groups, gates, or owner-side apply steps make the chain controllable and debuggable. If a reviewer cannot quickly tell what owns a value, what it is related to, whether it is meant to mutate, or when it changes, split the concept, rename it, or document the provisional risk before promoting the design.
- A good Loka abstraction should make application code feel simple without hiding where state lives or who cleans it up.
- Favor meaningful application code: app-facing APIs should express intent, ownership, and state flow rather than platform IDs, encoding buffers, or other incidental mechanics. Push platform-specific details down behind neutral names and thin value types when doing so keeps lifecycle and ownership understandable.
- Framework-facing names and contracts must not depend on vibes or leave room for competing interpretations. If users or maintainers could reasonably disagree about what an API means, tighten the name, split the concept, or make the ownership/lifecycle/dirty-flow contract explicit in the type shape before promoting it.
- Use `/** ... */` for class, struct, and API documentation comments that describe shipped concepts or contracts. Use `//` for local implementation notes.
- Avoid cleverness that only shortens code. Prefer clarity that makes the next feature easier to place.
- When an abstraction works, it should reduce future decisions, not create new special cases.
- Treat "magic" as acceptable only when the underlying structure remains inspectable and explainable.

## DSL And Composition
- Loka compose should use DSL-style chaining; avoid local temporary variables when possible.
- Prefer `this->` for member access; keep it consistent across the codebase.
- Prefer `deferBind` for UI reflection or lazy updates; use `bind` only when immediate recompute is required.
- DSL design: keep composition owned by Boundary; avoid extra compose layers unless needed. Use `Fragment` or helper functions returning node definitions to inline into the parent composition when you don't need an independent lifecycle.
- UI props constant-value policy: do not route DSL constant props through shared static `State<T>` helpers. For values such as button/cell text or menu enabled flags, props/definitions should own the constant value directly and only use `State<T>*` when live updates are actually required.
- Native binding policy: `PlatformController`/`NativeContext` code should bind only states that the logical node layer has classified as live. Avoid re-deciding liveness in platform code except for defensive guards.
- NativeContext should guard against null/empty state before drawing or binding.
- RTTI (`dynamic_cast`) is prohibited in DSL/scene code due to severe performance impact on 68k. Use virtual methods (`asXxx()`) or `NodeKind` checks instead. Add new `asXxx()` methods to Node when type-specific access is needed.
- Attr policy (68k): keep default attr structs as small PODs (target roughly <= 16-32 bytes). Avoid embedding heavy owned data in default attrs; route heavier payloads through explicit extended/pro attr types or external state handles.
- DSL props API policy: `Props` is the canonical/full API surface. `Definition` setters are optional shorthand only for frequently used fields in DSL call sites.
- DSL shorthand policy: for common cases, prefer concise `Definition` constructors/factories that accept the most-used inputs; for less common fields, construct `Props` explicitly and pass it through rather than duplicating every setter in both `Props` and `Definition`.

## Platform Rules
- UI layers should follow platform-native naming and conventions; core stays neutral.
- Classic Mac UI uses Toolbox/Control Manager APIs; avoid Carbon/Cocoa in Classic paths.
- Classic stability: avoid transient data in DSL props (e.g., pass stable pointers/references); if props own data, ensure copy/clone rebinds internal pointers safely.
- macOS support policy: library/core implementation targets Tiger through Snow Leopard compatibility; consumer applications are expected to run on Big Sur and newer and may integrate modern Swift/C++ features at the app layer.
- Consumer-application boundary: the rule above applies to applications built on top of Loka, not to this repository's library/core/example/platform code unless a repository-specific rule says otherwise.
- macOS architecture policy: default local debug/test builds in `CMakePresets.json` and `.vscode` should follow the host's native architecture (do not pin `CMAKE_OSX_ARCHITECTURES` for the default debug path). Cross-arch/universal release work should use the dedicated external scripts such as `ub2`, not the default VSCode debug tasks.
- macOS Objective-C policy: library/core code must stay ObjC1-style manual memory management (non-ARC); `@property`/`@synthesize` are allowed, but direct ivar access (`obj->ivar`) is forbidden and internal state access must go through private getter/setter methods.
- macOS examples policy: example targets default to non-ARC for consistency with library/core; ARC is allowed only as an explicit per-target opt-in when integration requirements demand it.
- Objective-C property policy (library/core): when using `@property`, always declare explicit ownership semantics (`retain`/`assign`/`copy`) and avoid implicit/default memory behavior.
- Objective-C access-scope policy: the no-direct-ivar-access rule applies to all library/core Objective-C(++) code under `apple/macos/src`; examples may diverge only when explicitly documented.
- macOS support terminology: mark compatibility claims explicitly as either `build-verified` (compiles/links) or `runtime-verified` (launched/tested on target OS/hardware); do not mix these terms.

## Performance And Retro Targets
- Toolbox/68k binary size policy: avoid `std::fstream`/iostream-based file I/O in Classic paths because it pulls large libstdc++ locale/stream machinery; prefer C stdio (`fopen`/`fread`) or platform file APIs.
- Prefer intrusive linked lists over `std::vector` when elements are heap-allocated anyway; adding a `next_` pointer avoids separate allocations and reallocation costs. On 68k, this primitive approach often outperforms "smart" containers.
- When users report performance issues or ask for speedups, first measure or propose a measurement plan; profiling support already exists in the codebase.
- Performance targeting policy: optimize primarily for 68030-era hardware and the repository's supported platform baselines; avoid regressions on low-end 68k (68000/68020) targets, but do not force 68k-first micro-optimizations without evidence.
- On 68k hot paths, avoid `StateStream` unless justified; manual `bind` + compute can be significantly faster for startup/compose.
- When profiling multiple sections inside one function, use `PROFILE_SECTION_ID` to avoid `__LINE__` collisions.
- Performance triage steps: 1) reproduce on modern OS with profiling on, 2) capture tick breakdown, 3) isolate by commenting out components or toggling features, 4) optimize top hotspots first, 5) re-measure, 6) record findings in docs/TODO.md.
- Redraw/performance triage: first identify whether cost comes from scene/update routing, boundary-local apply, or platform-specific fallback invalidation. Prefer measuring real redraw triggers before attempting dirty-rect shrinking.
- Classic/68k redraw policy: when broad repaint remains, prioritize suppressing redundant follow-up redraw triggers before fine-grained dirty-rect tuning.
- Classic/68k optimization order: first remove redundant state updates and compose passes (`forceUpdate`, unused state writes, extra Boundaries), then reduce redraw area, and only then add platform-specific dirty-region tricks.
- For animated Classic UIs, prefer quantized output gating before expensive updates: if integer-position/rendered output is unchanged, skip rebuilding props/models and avoid `MutableState::set()`.
- On Toolbox hot paths, prefer simple `NodeContext`-local previous-state caching over pushing detailed dirty metadata through shared DSL/app models when the optimization is platform-specific.
- For moving-rect redraw on Classic, `erase old minus new` is a safer first optimization than `paint new minus old`; only add more aggressive paint diffing after measurement proves it helps.

## Debugging And Review
- After completing a task, quickly review the diff for dangerous signs or design smells before considering it done: unclear ownership, duplicated cleanup paths, half-updated state, flag proliferation, or other structure that feels harder to reason about than before.
- Before opening, committing, or reviewing a non-trivial change, run a lightweight complexity gate. Count risk flags rather than trying to compute a perfect metric: new owner/lifecycle/cleanup paths; memory not clearly owned by a Boundary/owner scope or explicit higher-level owner; multiple new member fields or mutable flags; changes spanning two or more of State/Flow/Boundary/Platform; new cross-boundary state/callback/pointer sharing; raw callback/thunk lifetime that is not visible from the owner type; State update chains that can bounce between owners without a Flow/gate/interaction-group/owner-apply control point; dirty/layout/attach/detach/teardown behavior changes; ambiguous public API names; new fallback/default/implicit behavior; platform-specific workarounds leaking into common/app-facing code; or lifecycle/state-routing/layout/dirty behavior changes without tests.
- Complexity gate guideline: 0-1 flags is normal; 2-3 flags needs careful review and usually tests or explanatory comments; 4+ flags should be split or explicitly documented as provisional/follow-up work in `docs/TODO.md`, `ROADMAP.md`, or a narrow design note; 6+ flags should be rejected unless there is a strong design note and explicit owner approval. Unexplained complexity is a review failure even when the code works.
- Hard stop unless explicitly justified: new `dangerously*` usage; cross-boundary mutable state sharing; Node/Boundary directly owning platform handles without a projection seam; raw callbacks/thunks stored across Boundary lifetimes; update-loop or lifecycle behavior changes without tests or TODO coverage; or a public API whose ownership/lifecycle meaning is ambiguous.
- If a crash occurs, first confirm whether it reproduces on a modern OS build; use breakpoints, LLDB commands, and targeted logging to identify the cause quickly.
- For runtime behavior regressions in compose/dirty routing, prefer path verification before speculative edits: place breakpoints on the user action handler, observed-state thunk(s), `Scene`/`PlatformController` apply path, and the target node's `composeWithContext`/`composeNode`, then confirm where the flow stops.
- When debugging, assume there may be multiple possible failure points. Do not get stuck on a single suspected location; after a few attempts without meaningful progress, move to another candidate area and re-check where the failure actually begins.
- Add debug logs to clarify which stage is actually being reached, so they help decide where to look next rather than only reinforcing one hypothesis.
- If a request is ambiguous, stop and ask before implementing.
- If a design or implementation path looks fragile, hard to reason about, or likely to cause intermittent bugs, prefer a small refactor toward a simpler structure first. If no clear low-risk refactor is apparent, stop and call it out to the user before building further on top of it.

## Build, Test, And Commit
- Retro68 workflow policy: if `.dsk` is mounted (emulator/host), unmount it before rebuild. Building while mounted can leave stale artifacts or produce corruption-like runtime issues; when users report corrupted/unchanged Classic output, first retry with `.dsk` unmounted, then rebuild.
- Keep commits scoped; split large refactors into small, reviewable commits with verification between steps.
- Ask for runtime verification before commits that affect behavior (unless the change is clearly non-runtime, such as docs/comments/refactors that cannot affect execution).
- Commit policy: Do not amend commits. Only small fixes found immediately after a commit may be amended.
- Build output policy: keep all generated build trees under `build/` using purpose/platform-specific subfolders (for example `build/Testing`, `build/macos/Debug`, `build/retro68/68k/Release`); do not introduce ad hoc top-level build directories such as `build-foo`.
- Test build output policy: use `build/Testing` as the canonical test build directory (`cmake -S . -B build/Testing -DTEST_BUILD=ON`, then build/ctest from there).
- When a directly runnable test environment is available (e.g. Linux/WSL headless), always build and run the relevant tests before committing code or test changes. Do not skip this step even for "obviously correct" changes.
- Test-only introspection/access APIs should not expand normal prod-facing surfaces; prefer isolating them under a dedicated `testing` namespace/access layer (or equivalent backdoor) rather than adding general-purpose getters for tests.
- When adding a new example target, update `.vscode/launch.json` to include its run config.
- When adding a new example target, update `.vscode/tasks.json` so the matching build task exists for `preLaunchTask`.
