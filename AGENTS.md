# Project rules

- C++98 only; avoid newer syntax unless explicitly justified for design discussion.
- Prefer compile-time errors over runtime checks; leverage templates, inheritance constraints, and SFINAE to catch misuse at build time.
- Use TypeTag static checks in debug builds; allow overriding with `USE_LOKA_STATIC_ASSERT`. Prefer `static_assert` when C++11+ is available; in C++98 builds, keep them behind `LOKA_*_CHECK_TYPETAG`.
- C++ exceptions are disabled; do not add `try`/`catch` or rely on throwing.
- Prefer explicit error handling and nothrow/nullable patterns in Classic builds.
- Use `assert` for contract violations (e.g., null PlatformContext); do not throw.
- Rename mission: user-visible/app-layer strings and titles should move to "Loka";
  core namespaces/types remain "Declara" until a safe refactor permits change.
- UI layers should follow platform-native naming and conventions; core stays neutral.
- Classic Mac UI uses Toolbox/Control Manager APIs; avoid Carbon/Cocoa in Classic paths.
- MutableState<T>::set() must be wrapped in a StateTracker transaction (use RAII guard).
- Loka compose should use DSL-style chaining; avoid local temporary variables when possible.
- Prefer `this->` for member access; keep it consistent across the codebase.
- Prefer `deferBind` for UI reflection or lazy updates; use `bind` only when immediate recompute is required.
- Classic stability: avoid transient data in DSL props (e.g., pass stable pointers/references); if props own data, ensure copy/clone rebinds internal pointers safely.
- NativeContext should guard against null/empty state before drawing or binding.
- RTTI (`dynamic_cast`) is prohibited in DSL/scene code due to severe performance impact on 68k. Use virtual methods (`asXxx()`) or `NodeKind` checks instead. Add new `asXxx()` methods to Node when type-specific access is needed.
- Keep commits scoped; split large refactors into small, reviewable commits with verification between steps.
- Ask for runtime verification before commits that affect behavior (unless the change is clearly non-runtime, such as docs/comments/refactors that cannot affect execution).
- If a request is ambiguous, stop and ask before implementing.
- Secrets/PII must not be hardcoded; use env vars and avoid logging sensitive data.

## 68k Performance Optimization

### Profiling
- Use `core/Profiler.hpp` for cross-platform timing; Toolbox uses `TickCount()` (1 tick = 1/60 sec ≈ 16.7ms).
- `ScopedProfile` for block timing; cumulative counters (`gTreeVirtTicks`, etc.) for recursive operations.
- Profile results can be displayed on-screen via `ToolboxScenePlatformController`.

### Known Costs (measured on 68k)
| Operation | Approximate Cost |
|-----------|------------------|
| GroupNode/Boundary wrapper | ~30 ticks |
| useState (each) | ~10 ticks (heap alloc + vector push + tracker) |
| Floating point arithmetic | ~30 ticks (software FPU emulation) |
| dynamic_cast | Severe (use asXxx() instead) |

### Optimization Patterns
- **Inline content**: Avoid unnecessary GroupNode/Boundary wrappers; inline children directly into parent.
- **Pre-compute values**: Calculate floating-point at compile time or init; store as fixed strings.
- **reserveStates(n)**: Call at start of `attachNode()` to pre-allocate vector capacity.
- **addStateUnchecked**: Used internally by `useState()`; skips O(n) duplicate check.
- **String::Literal()**: Use for compile-time constant strings; avoids heap allocation.

### Example: BMI Optimization
Before (178 ticks):
```cpp
// GroupNode wrapper + 3 useState + floating point
<< BmiCalculator()
```

After (72 ticks):
```cpp
// Inline with pre-computed value, 1 useState
c.reserveStates(5);
bmiResult_ = c.useState<loka::core::String>(String::Literal("BMI: 20.76"));
// In composeNode:
<< Text("BMI Calculator")
<< Text("Height: 170cm")
<< Text(this->bmiResult_)
```
