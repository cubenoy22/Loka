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
- Node種別の分散化: NODE_KIND enum + asXxxNode() + PlatformController switch の一極集中を解消。各ノードクラスが自身のコンテキスト生成を持ち込める登録型アーキテクチャへ移行する。68k以外はRTTIでasXxxNode()を廃止、コンテキストファクトリ登録でenum/switchも不要にする。ユーザー定義コンポーネントの拡張性確保が目標。
- ImageView rendering policy: keep platform contexts on custom drawing paths (NSView/GDI/Toolbox) and avoid tying behavior to NSImageView-specific features for cross-platform parity.
- ImageView scaling contract: `fit` mode handling aligned on Win32/macOS (`NONE/CONTAIN/COVER/STRETCH`); Toolbox now has ImageView render path (frame + loaded-state overlay), but native image blit from `Image::nativeHandle` is still pending.
- Win32 image backend split plan: keep legacy GDI path for XP/2k compatibility and add capability-based modern path (WIC/high-DPI aware) without OS-name hard forks.
- Win32 compatibility note for planning: treat XP/2k as legacy baseline (build-verified target) and Vista+ as modern-capability tier; record per-feature support in the matrix.
- Flow DSL use-case validation: keep `Step(id, adapter)` focused on bounded-lifetime async/transform pipelines and verify readability/testability.
- Flow DSL composition hygiene: avoid one-step flows by default; split into 2+ steps at meaningful boundaries (transform/decision/side-effect) or use a plain function when flow orchestration value is absent.
- Flow DSL use-case validation: add video encoder stub scenarios (Qt / AVFoundation / Windows API style) for `open -> frame push -> finalize` and failure-path coverage.
- NodeDSL control components: prioritize `Cond`/`ShowIf` for OS-specific component branching; keep platform `#if` isolated in capability/platform implementation layers.
- Control components roadmap: `ShowIf`/`Cond` first (compose-time branch fixup), then constrained `For` (static/fixed list) to avoid 68k runtime loop costs.
- Cond/ShowIf test matrix: cover true/false branch selection, default/otherwise branch, nested Cond evaluation order, and stable node tree creation across recomposition.
- Cond/ShowIf state-change tests: verify branch switching when bound `State<bool>` changes, no stale active node retention, and expected cleanup/destructor calls on old branch nodes.
- Cond/ShowIf platform-branch tests: validate OS-specific component selection via stubs (Toolbox/Win32/macOS) without DSL-side `#if`.
- Layout alignment tests: validate `VStack` horizontal alignment and `HStack` vertical alignment defaults/overrides with deterministic node bounds.
- Text overflow tests: validate `TextAttr` wrap/truncation (`none/word`, `none/clip/ellipsis`) under constrained width and confirm `isClipped` does not replace text overflow policy.
- Wire `VStack/HStack` alignment props into platform layout engines (Win32/macOS/Toolbox) with consistent default behavior and fallback for unsupported native controls.
- Wire `TextAttr` overflow (`wrap`/`truncation`) into native text contexts (Win32 STATIC/AppKit NSTextField/Toolbox draw path) with explicit per-platform limitation notes.
- Row alignment measure pass currently does a 2-pass scan when vertical alignment is enabled; keep as-is for now, revisit for large-child optimization if profiling shows cost.
- Motion architecture note: keep `attr()` static and model time-based behavior in `Flow`/`State` (`TimeController`, timeline/Frame/delay chain). Backends resolve execution quality (GPU interpolation vs CPU step/invert fallback) via capabilities.
  - Sketch:
    `TimeController(ctrlState)`
    `  | Step(STEP_BLINK_ON, Frame(InvertRgn(target)).delayMs(1000))`
    `  | Step(STEP_BLINK_OFF, Frame(InvertRgn(target)).delayMs(1000))`
    `  | onSuccess(Handler::noop, STEP_BLINK_ON);`

## Completed (recent)

- Support matrix in README: split compatibility claims into `build-verified` and `runtime-verified`.
- Menu bar support implemented across macOS/Win32/Toolbox app layers.
- MenuItemAttr `visible` is evaluated during menu build. Runtime visibility toggles currently require menu invalidation/rebuild to reapply.
- Introduce `loka::multimedia` layer for codec/media responsibilities (ImageDecoder/Audio/Video), keeping `app` layer UI-only. Platform contexts should call multimedia abstractions instead of embedding QuickTime/AVFoundation/Win32 decode logic directly.
- Classic Toolbox image roadmap: move JPEG/PNG decode through QuickTime-backed multimedia decoder, then feed `Image` as normalized native handle (PICT/GWorld policy decided in multimedia).
- ConditionalDefinition/ConditionalNode and `NodeComposition::conditional(..., node)` default false/Empty path implemented.
- BoundaryNode owns StateTracker; useState auto-registers; Context API removed; RootBoundaryWrapper in Scene; DSL naming cleanup.
