# Boundary Memory Draft

This draft captures the intended direction for Loka's more complete memory and
resource ownership model. It is not a stable API contract yet.

## Goal

Loka already treats `Boundary` as the owner for state, Flow, composition, and
local resources. That ownership must become strong enough that applications can
build larger document and media models without scattering `new` / `delete`,
hidden reference counts, or app-local lifetime flags across the codebase.

The target model is:

- `Boundary` owns state, Flow, and boundary-local resources.
- Domain root objects such as `Movie`, `Document`, or `Project` can be held by a
  clear owner such as a `Boundary`, Repository, or ApplicationScope.
- Internal allocations inside a domain object remain normal C++ ownership unless
  there is a concrete reason to make them Loka-managed.
- Loka provides enough owner-aware handles that applications do not have to
  invent ad hoc reference counting or delayed deletion patterns.

## Boundary Lifetime

Boundary-owned resources are valid only while their owner Boundary is in an
alive phase. Teardown must be deterministic and child-first:

1. Child Boundaries detach and release before their parent Boundary.
2. Parent Boundary resources release only after child resources have had a
   chance to detach.
3. Scene root Boundary releases last.
4. Scene replacement through `SceneManager` must preserve the same ordering for
   the old Scene before the new Scene becomes the active owner of UI resources.

During teardown, creating new boundary-owned state, Flow, or resource holds
should be treated as a contract violation in debug/test builds.

## Release Timing

Immediate release is not always safe. Native platform callbacks, deferred Flow
work, and same-tick composition/apply code may still be unwinding when a
Boundary starts to detach.

The default release policy should be deferred unless a resource explicitly opts
into immediate release:

- `ReleaseNextTick` or equivalent should be the safe default for Boundary-owned
  state, Flow, and managed resources.
- `ReleaseImmediately` should be explicit and reserved for resources whose
  teardown is known to be safe in the current phase.
- A Scene-level or App/SceneManager-level dispose queue must guarantee that
  deferred releases still run when an entire Scene is replaced.

The final names are not decided. Candidate policy markers include:

- `ReleaseNextTick`
- `ReleaseImmediately`
- `UntilUnowned`
- `UntilUnownedNextTick`

The name must not imply that a resource is disposed at a specific time if
another Boundary still holds it.

## Managed Handles

`Managed<T>` should probably remain available because it is useful as a small
handle for shared domain roots, decoded media, image records, and future
document-like objects. However, it should not become an anonymous reference
counting box.

Preferred direction:

- `Managed<T>` stores or points to a small control block.
- The control block tracks which Boundaries or owner tokens currently hold the
  payload.
- A small fixed owner-slot count is likely enough for most UI/document cases;
  overflow behavior must be explicit.
- `WeakRef<T>` can observe the payload without keeping it alive.
- `StrongRef<T>` or equivalent may exist, but its relationship to owner slots
  must be clear. Copying a strong handle must not silently create confusing
  ownership.

For debugging, the important question should be "which owner is still holding
this?" rather than "why is the count nonzero?"

## Boundary Holds

A boundary hold API should make the owner relationship visible without exposing
raw Boundary internals to ordinary app code.

Possible shape:

```cpp
Managed<Movie> movie = holdInBoundary(new Movie(), UntilUnownedNextTick);
```

The exact naming is open. `Hold` currently reads better than `Keep` because it
implies an explicit owner relationship rather than cache-like persistence.

Important semantics:

- A hold belongs to a Boundary or owner token.
- Releasing one Boundary's hold does not dispose the payload if another owner
  still holds it.
- The payload is disposed only after it becomes unowned, then according to the
  selected release timing.
- Boundary pointers, if stored internally, are owner identity only. Managed code
  should not traverse Boundary graphs through those pointers.

## Domain Objects

Loka should not force every internal allocation inside a domain model to become
Boundary-managed. A `Movie` may own frames, effects, buffers, and decoded data
with normal C++ ownership internally.

Loka should manage the root object and integration points:

- which Boundary/Repository/ApplicationScope owns the root object;
- which UI reads it;
- which facade or Flow mutates it;
- which state/event reports that it changed;
- when the root object and related resources are released.

This keeps domain code portable and testable without making it unnecessarily
dependent on Loka internals.

## Open Questions

- Should owner slots store `BoundaryNode *`, a generic owner token, or both?
- What is the default owner slot count, and what happens on overflow?
- Should deferred disposal be owned by `Scene`, `SceneManager`, or `App`?
- When a weak reference observes an object queued for next-tick disposal, should
  it return null immediately or remain readable until the dispose queue flushes?
- How should Repository/ApplicationScope integrate with the same hold model?
- Which resources are safe to release immediately in Classic Toolbox, Win32,
  and macOS platform paths?

