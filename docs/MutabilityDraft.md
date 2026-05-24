# Mutability Draft

This draft captures early design notes for immutable values, mutable builders,
resource buffers, and owner-aware mutation. It is not a stable API contract.

## Principle

Mutability should be visible from the type shape.

Loka should avoid APIs where an object looks read-only but may secretly be
mutable, or where callers must ask `isMutable()` before they know what operations
are safe. That style makes review-by-shape difficult: readers cannot tell
whether a value is a completed fact, a construction buffer, or an owner-local
mutable resource without tracing runtime state.

Prefer distinct types:

```text
Blob                 immutable/read-only byte value
MutableBlob          owner-local mutable byte buffer
BlobBuilder          construction-only writer

Image                immutable logical image handle
ImageRep             decoded/platform/backend representation
MutableImageBuffer   owner-local mutable pixel buffer
ImageBuilder         construction-only writer
```

Names are not final. The important rule is that a completed value and a mutable
construction/editing buffer should not be the same app-facing type.

## Completed Facts

Completed facts should be immutable-like:

- props;
- snapshots;
- apply plans;
- completed Blob values;
- logical Image handles;
- decoded resource keys;
- finished document/model fragments where practical.

Immutability does not always require deep physical copying. A value may hold a
shared payload or owner-aware handle. However, app-facing mutation should not be
available through the completed type.

## Mutable Phases

Mutation belongs in an explicit phase:

- builder;
- local temporary;
- owner-local mutable buffer;
- Flow step;
- platform/resource adapter;
- Boundary/Repository-owned state machine.

After construction or editing, the result should become a completed value, or a
mutation request should be applied through the owner that is responsible for the
state.

Example shape, not final API:

```cpp
BlobBuilder builder;
builder.append(bytes, length);
Blob blob = builder.finish();
```

or:

```cpp
MutableImageBuffer pixels(owner);
drawInto(pixels);
Image image = pixels.freeze();
```

Mutable access should be a fallible operation, not a boolean property or a
`MutableState<bool>` marker. A completed value should not say "I am currently
mutable" through runtime state; instead, code should explicitly request a
mutable editing handle from an owner.

Possible shape, not final API:

```cpp
MutableBlobAccess access = blob.tryEdit(owner);
if (!access.ok())
{
  return access.error();
}
access.value()->append(bytes, length);
Blob updated = access.freeze();
```

Names such as `tryEdit`, `beginEdit`, `acquireMutable`, or `moveAsMutable` are
still open. The required semantics are:

- success returns an owner-registered mutable access/buffer;
- failure is explicit and typed, not hidden behind a null pointer or silent no-op;
- possible failures include out-of-memory, owner mismatch, locked resource,
  unsupported mutation, and stale owner/lifecycle;
- mutable access has a visible owner scope and cleanup path;
- the final result is either frozen/applied or discarded.

This should work with Flow. A Flow step should be able to request mutable
access, branch on success/failure, edit, freeze, and apply the result through the
owning state/action. This keeps Classic no-exception builds honest while still
allowing memory pressure to be handled as data.

## Boundary Ownership

Mutable resources should have visible ownership.

`Blob` and `Image` can be shared across Boundaries more safely when they are
immutable completed values. `MutableBlob`, `MutableImageBuffer`, or similar
types should belong to a Boundary, BoundarySection, Repository, ApplicationScope,
or another explicit owner. Passing a mutable buffer across Boundary lines should
be treated as a design event, not routine implementation.

Default direction:

- immutable completed values may be shared;
- mutable buffers are owner-local;
- cross-boundary writes go through an owner method, Flow, action, or facade;
- freeze/copy should be explicit when a mutable result leaves its owner.

## Copy-On-Write

Copy-on-write may be useful, but only if the ownership rule stays visible.

Because Loka can often know which Boundary owns a state/resource, COW could be
safe when a value crosses owner boundaries: a different Boundary mutating a
shared payload would first receive an owned copy.

The same does not make mutation safe inside one Boundary. If several pieces of
logic in the same Boundary share a mutable payload, COW will not make the
relationship obvious. It can hide who changed a value and why. Prefer explicit
mutable buffers or owner-side apply steps inside one owner scope.

Possible rule:

- COW across different owners can be allowed when the copied owner is explicit;
- COW inside the same owner should not be used as a substitute for clear state
  ownership;
- COW must not make a completed immutable type appear mutable to ordinary app
  code.

## Blob Direction

`Blob` should likely become an immutable byte value or immutable shared byte
handle. It should represent completed bytes.

Open questions:

- Should `Blob` own bytes directly, use shared payload, or wrap a future
  `Managed<T>`-style resource handle?
- Should `BlobBuilder` be the only append/write path?
- Should `MutableBlob` exist separately from `BlobBuilder`, or is a builder
  enough for the first use cases?
- How should `Blob` expose raw bytes on Classic targets without pulling in
  heavy abstractions?

## Image Direction

`Image` should likely be a logical immutable image handle.

Decoded pixels, platform image handles, textures, WIC/NSImage/QuickDraw
resources, and cached representations should live in resource/platform scopes,
not inside ordinary app-facing `Image` mutation APIs.

Possible layering:

```text
Image
  logical immutable image value/handle

ImageRep
  decoded/backend/platform representation

MutableImageBuffer / ImageBuilder
  explicit mutable construction or editing buffer
```

This keeps app code readable while leaving room for platform-specific memory and
cache policy.

## Review Rules

When reviewing mutability-sensitive changes, ask:

- Does the type name reveal whether the value is mutable?
- Is the mutable phase narrow and owned?
- Can a completed value be changed after construction?
- Does a mutable buffer cross a Boundary without an explicit owner/facade?
- Is COW hiding a relationship that should be expressed as owner-side apply?
- Can resource cleanup be traced to a Boundary, Repository, ApplicationScope, or
  platform/resource cache?

If these answers are unclear, split the type, rename the API, or document the
provisional risk before promoting the design.
