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
- Avoid RTTI (`dynamic_cast`) in hot paths; prefer `NodeKind` checks. If RTTI is required, confirm with the user first.
- Keep commits scoped; split large refactors into small, reviewable commits with verification between steps.
- Ask for runtime verification before commits that affect behavior (unless the change is clearly non-runtime, such as docs/comments/refactors that cannot affect execution).
- If a request is ambiguous, stop and ask before implementing.
- Secrets/PII must not be hardcoded; use env vars and avoid logging sensitive data.
