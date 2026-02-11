# Loka TODO

## Open

- Define Modifier system (Text style + Window sizing) and wire through WindowProps/Layout.
- Decide default window size (macOS/Win32/Toolbox) and unify hardcoded values.
- Sample: Image viewer.
- Sample: MyTracker (editable device spec catalog).
- Sample: FloppyBird (FlappyBird clone).
- Audit remaining `std::string` usage and keep only intentional interop edges (UTF-8/platform bridge paths).
- Declarative OpenGL game foundation (mainline): scene/update/render flow, resource model, and platform bridge boundaries.
- Sprite path (Toolbox/Win32) as lightweight compatibility backend, not the primary game direction.
- Event loop + tick design for game-style rendering (separate from UI updates), aligned with the declarative game path.
- Window close request: delegate to Scene/Root.
- loka::core::Managed<T> circular reference patterns (Group/Weak or one-way ref policy).
- Re-evaluate ErrorSink requirement for Headless/Loader after ImageViewer integration; keep it optional unless concrete error-flow pain appears.
- MutableState notification timing: micro-tick end vs immediate; document or change.
- DerivedState::EvalFn ownership/cleanup and dependency registration policy.
- StateBatch: add useLargeStates path for large initializers (heap-backed initial copies).
- Wire Node.dirty with IPlatformController::synchronize (diff-based redraw path).
- Smart redraw scheduler at Window/Scene scope: collect dirty rects per tick and flush once via platform invalidate APIs.
- Toolbox/68k redraw policy: replace direct `drawDirty` calls with `markDirty` accumulation, then flush once in `updateEvt` (`BeginUpdate/EndUpdate`) using `InvalRect`/`InvalRgn`.
- Toolbox/68k profiling: count redraw triggers and merged dirty regions per interaction to remove duplicate invalidation paths before deeper redraw refactors.
- Props in/out pattern for `State<T>*` and `EmitterState*` props (bidirectional vs one-way).
- Decide where Scene.lifecycle\_ is written (SceneManager2 vs disabled).
- SceneManager2: finalize lifecycle updates (ON_DETACH/ON_ATTACH/ON_DESTROY) and make Scene subscribe-ready.
- SceneManager2: review API for swapping to CompositeNode/NodeComposition once Window::mount lands.
- requestDiscard protocol for save/confirm flows via EmitterState.
- SceneManager2 tests for pendingTransactions\_ behavior and Tracker integration.
- Suppress clangd incomplete type warning for AttachedContext -> BoundaryNode access (include or type split).
- C++98: down-port tests/SceneTests.hpp (no lambda/auto/override) or exclude for legacy builds.
- C++98: reintroduce NodeComposition map/filter via C++98-friendly adapters.
- Toolchain matrix: validate oldest MSVC/GCC and record gaps.
- Docs/tests: document C++98 constraints and add checks for accidental C++11 usage.
- Cleanup staged work from earlier C++98 retrofit (split/rebase if needed).
- DSL shorthand ideas: direct props overloads (Text("...")), direct State props (EditText(State*)), optional prepare/compose merge, namespace alias, Fragment helper.

## Completed (recent)

- Support matrix in README: split compatibility claims into `build-verified` and `runtime-verified`.
- Menu bar support implemented across macOS/Win32/Toolbox app layers.
- ConditionalDefinition/ConditionalNode and `NodeComposition::conditional(..., node)` default false/Empty path implemented.
- BoundaryNode owns StateTracker; useState auto-registers; Context API removed; RootBoundaryWrapper in Scene; DSL naming cleanup.
