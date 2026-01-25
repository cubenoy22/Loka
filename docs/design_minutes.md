# Loka Design Minutes (Solid-mode)

Short design notes to keep the "why" and current direction in one place.
Detailed usage lives in `docs/DSL.md` and `docs/Reactive.md`.

## Summary

- Prefer Solid-like reactive updates over global diff/apply.
- Boundaries own state; non-boundary nodes delegate to the nearest boundary.
- Keep C++98, no exceptions, no RTTI in DSL/scene paths.

## State + Tracker

- `State<T>` / `MutableState<T>` / `EmitterState` are the core primitives.
- `PushStateTracker` provides transaction boundaries and dependency propagation.
- `loka::core::StateTrackerGuard` is required when mutating state.

## NodeComposition + Arena

- Composition clones definitions into an arena; temporary DSL objects are safe.
- `declare()` builds the tree; `createNodeTree()` materializes nodes.
- `group()` stores a definition for reuse without manual ownership.

## Resources

- `loka::core::Managed<T>` is the shared-handle wrapper for platform resources.
- Keep ownership with the longest-lived object (Scene/Window/Boundary).

## Known Gaps (Keep in TODO)

- `Node.dirty` diff-based redraw plumbing
- Scene lifecycle update semantics
- Loader error flow (ErrorSink)

Last updated: 2026-01
