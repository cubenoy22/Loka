# Project rules

- C++98 only; avoid newer syntax unless explicitly justified for design discussion.
- Prefer compile-time errors over runtime checks; leverage templates, inheritance constraints, and SFINAE to catch misuse at build time.
- C++ exceptions are disabled; do not add `try`/`catch` or rely on throwing.
- Prefer explicit error handling and nothrow/nullable patterns in Classic builds.
- Use `assert` for contract violations (e.g., null PlatformContext); do not throw.
- Rename mission: user-visible/app-layer strings and titles should move to "Loka";
  core namespaces/types remain "Declara" until a safe refactor permits change.
- UI layers should follow platform-native naming and conventions; core stays neutral.
- Classic Mac UI uses Toolbox/Control Manager APIs; avoid Carbon/Cocoa in Classic paths.
- MutableState<T>::set() must be wrapped in a StateTracker transaction (use RAII guard).
- Prefer `this->` for member access; keep it consistent across the codebase.
- If a request is ambiguous, stop and ask before implementing.
- Secrets/PII must not be hardcoded; use env vars and avoid logging sensitive data.
