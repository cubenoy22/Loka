# Loka TODO

## Highly recommended

These items address recurring bug patterns and structural risks identified during recent bugfixes (ConditionalDefinition dangling pointer, Mac platform context preservation, startup redraw).

- **Definition ownership clarity**: `clone()` returns raw `new`-ed pointers across 19+ call sites with implicit ownership. Introduce a lightweight `OwnedDef<T>` wrapper (C++98-compatible) so ownership intent is visible in code. Priority: prevents the same class of bug as the ConditionalDefinition fix.
- **Platform Controller layout共通化**: `layoutNode()` is near-identical across Mac/Win32/Toolbox (~2000 lines each). Extract platform-independent layout traversal and size calculation to shared code; platform-specific parts (NSView/HWND/QuickDraw context creation) become overrides. Priority: bug fixes (like removing `clearContexts()`) currently need manual porting to 3 implementations.
- **Portable platform controller tests**: Mac/Win32 layout/context tests only run on their native platform. Abstract layout calculation into a testable interface so core layout logic can be verified on Linux CI.
- **Composition event tracing**: Add `LOKA_TRACE_COMPOSITION` macro to log ATTACH/UPDATE/DETACH/RETAIN/REPLACE transitions in `DynamicCompositionBoundaryNodeBase::composeWithContext()` and `applyLocalRebuildPlan()`. Disable on Retro68. Priority: reduces manual printf debugging during composition bugs.

## Open

- Define Modifier system (Text style + Window sizing) and wire through WindowProps/Layout.
- Decide default window size (macOS/Win32/Toolbox) and unify hardcoded values.
- Sample: MyTracker (editable device spec catalog).
- Sample: FloppyBird (FlappyBird clone).
- Audit remaining `std::string` usage and keep only intentional interop edges (UTF-8/platform bridge paths).
- Declarative OpenGL game foundation (mainline): scene/update/render flow, resource model, and platform bridge boundaries.
- Sprite path (Toolbox/Win32) as lightweight compatibility backend, not the primary game direction.
- Pseudo-view sprite path: for lightweight game/decorative nodes, prefer one retained root native view plus scene-managed pseudo-views/sprites instead of one native subview per sprite. Keep `RectSurfaceNode`/similar nodes as normal scene nodes and push backend-specific diff/draw optimization into `NodeContext`.
- Event loop + tick design for game-style rendering (separate from UI updates), aligned with the declarative game path.
- Window close request: delegate to Scene/Root.
- loka::core::Managed<T> circular reference patterns (Group/Weak or one-way ref policy).
- Re-evaluate ErrorSink requirement for Headless/Loader after ImageViewer integration; keep it optional unless concrete error-flow pain appears.
- MutableState notification timing: micro-tick end vs immediate; document or change.
- DerivedState::EvalFn ownership/cleanup and dependency registration policy.
- StateBatch: add useLargeStates path for large initializers (heap-backed initial copies).
- Wire Node.dirty with IPlatformController::synchronize (diff-based redraw path).
- Smart redraw scheduler at Window/Scene scope: collect dirty rects per tick and flush once via platform invalidate APIs.
- Win32 redraw policy follow-up: keep `Text`/`Cell` immediate for correctness for now, but revisit a clearer control-type policy once transparent text/custom child repaint contracts are documented.
- Toolbox/68k redraw policy: replace direct `drawDirty` calls with `markDirty` accumulation, then flush once in `updateEvt` (`BeginUpdate/EndUpdate`) using `InvalRect`/`InvalRgn`.
- Props in/out pattern for `State<T>*` and `EmitterState*` props (bidirectional vs one-way).
- requestDiscard protocol for save/confirm flows via EmitterState.
- Menu rebuild contract: `MENU_ACTION_REBUILD_MENU` and menu-local state changes do not yet produce reliable reactive rebuild/apply across platforms, especially on macOS menu tracking. Add dedicated contract tests before expanding reactive menu samples.
- DSL definition lifetime safety: `ConditionalDefinition` now owns cloned trueDef/falseDef (fixed). Remaining: audit other definition types for similar raw-pointer-to-temporary patterns; see "Definition ownership clarity" in Highly recommended.
- Dynamic subtree granularity: sibling context preservation now works via local diff RETAIN + removal of `clearContexts()` from platform layout (fixed). Remaining: `NODE_DIRTY_CHILD` still rebuilds the whole dynamic boundary subtree; lighter-weight partial-child diff is a future optimization.
- Local dynamic diff follow-up: scene-side local rebuild planning is now split into comparison summary (`NodeCompositionDiff`) and boundary-local apply plan (`LocalRebuildPlan`). Remaining work is to decide how much of that kernel should be shared with menu diff/apply without forcing premature abstraction.
- Manual wiring pitfalls: explicit registration/findBoundary-style coordination is Loka-like but easy to miswire (for example, registering the wrong boundary instance or forgetting a required hook). Add small helper APIs, debug asserts, and example/platform smoke tests for high-risk manual paths such as freeze targets, boundary-local callbacks, and owner registration.
- Suppress clangd incomplete type warning for AttachedContext -> BoundaryNode access (include or type split).
- C++98: down-port tests/SceneTests.hpp (no lambda/auto/override) or exclude for legacy builds.
- C++98: reintroduce NodeComposition map/filter via C++98-friendly adapters.
- Toolchain matrix: validate oldest MSVC/GCC and record gaps.
- Docs/tests: document C++98 constraints and add checks for accidental C++11 usage.
- Cleanup staged work from earlier C++98 retrofit (split/rebase if needed).
- DSL shorthand ideas: direct props overloads (Text("...")), direct State props (EditText(State*)), optional prepare/compose merge, namespace alias, Fragment helper.
- C++98 chain-entry helper idea: add a thin wrapper/helper for DSL/Stream/Flow entry points so callers can keep chaining without spelling long intermediate types when `auto` is unavailable. Keep it narrow and purposeful rather than a broad "wrap anything" abstraction.
- Decentralize node-type dispatch: replace the single `NODE_KIND` + `asXxxNode()` + `PlatformController` switch concentration with a registration-based context factory model. The long-term goal is to remove enum/switch dispatch and most `asXxxNode()` paths outside 68k-sensitive builds while preserving user-defined component extensibility.
- Introduce `loka::multimedia` layer for codec/media responsibilities (ImageDecoder/Audio/Video), keeping `app` layer UI-only. Platform contexts should call multimedia abstractions instead of embedding QuickTime/AVFoundation/Win32 decode logic directly.
- ImageView rendering policy: keep platform contexts on custom drawing paths (NSView/GDI/Toolbox) and avoid tying behavior to NSImageView-specific features for cross-platform parity.
- ImageView scaling contract: `fit` mode handling aligned on Win32/macOS (`NONE/CONTAIN/COVER/STRETCH`); Toolbox now has ImageView render path (frame + loaded-state overlay), but native image blit from `Image::nativeHandle` is still pending.
- Classic Toolbox image roadmap: move JPEG/PNG decode through QuickTime-backed multimedia decoder, then feed `Image` as normalized native handle (PICT/GWorld policy decided in multimedia).
- SimpleViewer (Toolbox): add optional QuickTime decode path via `loka::multimedia` and surface explicit error codes for `QT_UNAVAILABLE` / `QT_DECODE_FAILED` when fallback PICT decode cannot render.
- Win32 image backend split plan: keep legacy GDI path for XP/2k compatibility and add capability-based modern path (WIC/high-DPI aware) without OS-name hard forks.
- Win32 compatibility note for planning: treat XP/2k as legacy baseline (build-verified target) and Vista+ as modern-capability tier; record per-feature support in the matrix.
- Flow DSL use-case validation: keep `Step(id, adapter)` focused on bounded-lifetime async/transform pipelines and verify readability/testability.
- Flow DSL composition hygiene: avoid one-step flows by default; split into 2+ steps at meaningful boundaries (transform/decision/side-effect) or use a plain function when flow orchestration value is absent.
- Flow DSL nesting/composition: design `RunFlow(child)` or equivalent child-flow invocation so parent scenarios can keep coarse-grained steps while low-level checks stay encapsulated in reusable subflows.
- Flow DSL combinators: revisit logical `all` / `race` after child-flow support lands; prioritize test/scenario semantics (grouped checks, timeout-vs-event wait) over concurrency.
- Flow DSL perf harness: add a measurement flow that runs an operation/scenario flow 3-10 times, records per-run timing/profile data, and exits with a summarized report so the same harness works on Toolbox/macOS/Win32.
- Flow DSL runtime quirk: investigate cases where adding `FlowChain::onFailure(...)` changes outcome/stability of otherwise equivalent scene tests; likely shared/clone/runtime-step state handling around failure callbacks.
- Platform apply taxonomy: revisit `PAINT_COMPOSITED` classification. A simple `ZStack`-presence heuristic was not sufficient even when the composed tree clearly contained `ZStack`; likely needs boundary-local paint/opacity metadata rather than scene-side structural guessing.
- Flow DSL use-case validation: add video encoder stub scenarios (Qt / AVFoundation / Windows API style) for `open -> frame push -> finalize` and failure-path coverage.
- Control components roadmap: add constrained `For` (static/fixed list) after `Cond`/`ShowIf`, while keeping platform `#if` isolated in capability/platform implementation layers and avoiding unnecessary 68k runtime loop costs.
- Cond/ShowIf remaining coverage: fill the gaps beyond the current basic branch-switch tests, specifically default/otherwise branches, nested evaluation order, cleanup/destructor behavior on branch replacement, and platform-selection stubs without DSL-side `#if`.
- Layout alignment tests: validate `VStack` horizontal alignment and `HStack` vertical alignment defaults/overrides with deterministic node bounds.
- Text overflow tests: validate `TextAttr` wrap/truncation (`none/word/char`, `none/clip/ellipsis`) under constrained width and confirm `isClipped` does not replace text overflow policy.
- Toolbox wrapped-text relayout behavior: runtime-verify that content-change triggers relayout correctly (build/test path implemented).
- Row alignment measure pass currently does a 2-pass scan when vertical alignment is enabled; keep as-is for now, revisit for large-child optimization if profiling shows cost.
- State scheduler idea: add `NextEventTracker` (next-event-cycle flush, setTimeout(..., 0)-like batching) to coalesce rapid `State::set()` bursts on Main Thread.
- Style follow-up: newer boundary state helper headers (`BoundaryStateTypes.hpp`, `BoundaryRuntimeState.hpp`, etc.) still mix direct member access and `this->`; align them with the repository-wide `this->` preference during future cleanup passes.
- C++98 RAII follow-up: `BoundaryComposePhaseScope` / `BoundaryApplyPhaseScope` still rely on copy-transfer via `mutable` + `const_cast`; keep usage narrow and revisit if a simpler non-copying scope pattern becomes practical.
- Animation scheduler: keep it separate from dirty routing. Use a lightweight frame/timeline scheduler that advances animation state, then let existing `State -> dirty` routing handle `PROPS/LAYOUT/CHILD`. Start with an active-animation list plus `NextTickTracker::request(delayMs)` rather than a generic global queue.
- Motion architecture note: keep `attr()` static and model time-based behavior in `Flow`/`State` (`TimeController`, timeline/Frame/delay chain). Backends resolve execution quality (GPU interpolation vs CPU step/invert fallback) via capabilities.
  - Sketch:
    `TimeController(ctrlState)`
    `  | Step(STEP_BLINK_ON, Frame(InvertRgn(target)).delayMs(1000))`
    `  | Step(STEP_BLINK_OFF, Frame(InvertRgn(target)).delayMs(1000))`
    `  | onSuccess(Handler::noop, STEP_BLINK_ON);`

## Completed (recent)

- Update/apply optimization pass (2026-03): archived in [docs/archives/update_pipeline_optimization_2026-03.md](archives/update_pipeline_optimization_2026-03.md).
  Highlights:
  - scene-level `fullRebuild` accuracy improved for child-dirty and mixed dirty paths
  - localized platform apply routing (`PlatformApplyPlan` / `BoundaryLocalApplyInfo`) is now the main redraw contract
  - Win32 interaction flicker and Toolbox follow-up redraw behavior were reduced by platform-local fixes
- Debug logging/build gating:
  - `LOKA_ENABLE_DEBUG_LOGS` now gates debug logging support
  - Release builds keep debug logs off
  - HelloWorld `Debug` menu is hidden in Release builds
- Toolbox redraw profiling support: HelloWorld can dump/reset Toolbox debug stats to timestamped files for local interaction analysis.
- Support matrix in README: split compatibility claims into `build-verified` and `runtime-verified`.
- Menu bar support implemented across macOS/Win32/Toolbox app layers.
- MenuItemAttr `visible` is evaluated during menu build. Runtime visibility toggles currently require menu invalidation/rebuild to reapply.
- Menu/Scene architecture convergence: keep boundary-local tracking, minimal diff output, and immediate-vs-deferred policy split aligned now that both scene and menu use `NextTickTracker`; remaining work is to converge scheduling entry points and shared semantics without duplicating dirty logic.
- ConditionalDefinition/ConditionalNode and `NodeComposition::conditional(..., node)` default false/Empty path implemented.
- BoundaryNode owns StateTracker; useState auto-registers; Context API removed; RootBoundaryWrapper in Scene; DSL naming cleanup.
- `VStack/HStack` alignment props are wired into platform layout engines (Win32/macOS/Toolbox), including remaining-height handling for `VStack + ImageView(FILL_PARENT)`.
- `TextAttr` overflow (`wrap`/`truncation`) is wired into native text contexts (Win32/macOS/Toolbox with low-memory-safe clip fallback).
- Scene local diff first pass: retained native contexts now survive local replace/reorder paths across generic/macOS/Win32 tests, retired subtree cleanup has a platform seam, and boundary-local rebuild planning is separated from apply.
- Scene `fullRebuild` accuracy is no longer root-only in the old sense; child-dirty and mixed dirty cycles now use boundary/root diff results to downgrade more aggressively when structure work is not required.
