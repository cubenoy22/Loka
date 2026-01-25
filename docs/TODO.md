# Loka TODO

## Open

- Define Modifier system (Text style + Window sizing) and wire through WindowProps/Layout.
- Decide default window size (macOS/Win32/Toolbox) and unify hardcoded values.
- Menu bar support vs. Retro68 deep support: pick next priority.
- Menu bar support (macOS/Win32/Toolbox).
- Sample: Image viewer.
- Sample: MyTracker (editable device spec catalog).
- Reduce remaining std::string usage where Loka::String is feasible.
- Toolbox game rendering: offscreen GWorld + CopyBits dirty-rect path (Sprite/Canvas baseline).
- Win32 game rendering: DIB back buffer + BitBlt dirty-rect path (Sprite/Canvas baseline).
- Event loop + VBL/timer tick design for game-style rendering (separate from UI updates).
- Window close request: delegate to Scene/Root.
- Managed<T> circular reference patterns (Group/Weak or one-way ref policy).
- Headless/Loader components should require ErrorSink for unified error flow.
- ConditionalDefinition/ConditionalNode implementation and NodeComposition::conditional default false/Empty handling.
- MutableState notification timing: micro-tick end vs immediate; document or change.
- DerivedState::EvalFn ownership/cleanup and dependency registration policy.
- StateBatch: add useLargeStates path for large initializers (heap-backed initial copies).
- Wire Node.dirty with IPlatformController::synchronize (diff-based redraw path).
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
- ErrorSink API sketch (push/tryPop/bindNewEvent).
- Add ImageLoader producer path using ErrorSink.
- Align ErrorSink push with StateTracker::defer timing.
- DSL shorthand ideas: direct props overloads (Text("...")), direct State props (EditText(State*)), optional prepare/compose merge, namespace alias, Fragment helper.

## Completed (recent)

- BoundaryNode owns StateTracker; useState auto-registers; Context API removed; RootBoundaryWrapper in Scene; DSL naming cleanup.
