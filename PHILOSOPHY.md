# Loka Philosophy

Loka is not only a GUI toolkit. It is intended to grow into a small, coherent
engine for applications, animation, video, and game production while keeping the
same core concepts meaningful across those domains.

## Software Longevity

Loka's retro-to-modern reach is not nostalgia for its own sake. The deeper goal
is to let people build software whose meaning can outlive a particular operating
system version, UI framework, compiler trend, device class, or distribution
channel.

Application logic, state ownership, composition, resources, timelines, and
interaction models should be described in Loka concepts as much as practical.
Platform layers should project those concepts into native behavior without
making the application itself depend on the platform's incidental shape.

Software should be able to age well. A Loka application should have a path to
keep running, be ported, be inspected, and be adapted without rewriting its core
meaning every time the surrounding environment changes.

## Software As Specification

Loka should help separate what software is from where it happens to run.

Modern frameworks often make application behavior tightly coupled to a specific
OS feature, runtime convention, UI toolkit, build system, or distribution model.
Loka should reduce that stress. Its APIs should let application code read more
like a durable specification of state, composition, resources, events, timelines,
and intent, while platform layers translate that specification into native
behavior.

The goal is not to avoid native integration. The goal is to keep native
integration from becoming the application's identity.

Application-facing headers and composition code should not inherit native
toolkit namespace pollution as an ambient cost of portability. Platform headers,
native type names, and compatibility shims belong behind platform projection
boundaries or narrow bootstrap seams. If an application author is writing a
logical `Button`, `Text`, `Window`, or future control, that expression should
mean the Loka concept by default, not whichever native SDK happened to be
included first.

## Meaningful Code

Application-facing code should express intent, ownership, state flow, and
composition. It should not force users to think in platform IDs, encoding
buffers, native handles, or other incidental mechanics unless they are explicitly
working at that layer.

When an API name can reasonably be read in more than one way, tighten the name
or split the concept. Framework code should not depend on vibes.

## Structure Over Vigilance

Correctness should be carried by structure, not by contributor care. When a
comment must warn that several loose fields only stay consistent if every code
path remembers to update or reset them together, that is not documentation; it
is a fragile invariant waiting for a refactor to break it. Group those fields
into a small type whose operations maintain the invariant, give resources a
dedicated owner that releases them by construction/destruction pairing, and let
the surrounding code shrink to policy that can be read as a short sequence of
decisions.

The test for such an internal boundary is what it deletes. A good extraction
removes duplicated bookkeeping, per-item copies of aggregate data, defensive
comments, and cleanup interleaved with unrelated work. The remaining types hold
only data that is meaningful at their own level. A wrapper that merely adds a
name and a level of indirection has not found a real boundary and should not be
kept.

The same discipline applies to mirrored logic. Reserve and allocate, create
and destroy, attach and detach, estimate and consume: when two code sites must
agree for the system to stay correct, that agreement is an invariant even
though no shared field connects them. Writing the second side by hand, from
memory, in a different file is how such pairs drift. Give the shared fact one
named home that both sides use, or keep the pair adjacent with an explicit
cross-reference. Parallel implementations across platform seams may stay
separate deliberately, but each unmarked twin is a porting hazard; divergence
should be a choice, not an accident.

This applies inside the framework as much as at app-facing surfaces. Retro
targets make hidden fragility expensive to debug, so the same pressure that
keeps application code explicit should keep internal mechanics encapsulated,
owned, and inspectable.

## Joyful, Unambiguous Authoring

Loka should be beautiful to design and pleasant to use. A person writing Loka
code should be able to stay close to the software they want to make, choose the
next expression without hesitation, and enjoy the feeling that the code is saying
what they mean.

This is not about hiding structure or making everything look short. It is about
removing accidental decisions so the remaining choices are meaningful. Good Loka
code should be readable after the fact as a record of intent, ownership, and
composition rather than a pile of framework ceremony.

## Small Concept Set

Loka should prefer a small set of reusable concepts over a new mechanism for
every feature:

- `Node` for meaningful units in a composition tree
- `Boundary` for ownership, lifecycle, composition, and dirty/update routing
- `State` and `StateTracker` for explicit reactivity
- `Props` for stable inputs
- `Flow` for bounded work, event pipelines, and future media/game operations
- platform projection for native integration without leaking platform details

This consistency matters more than short-term convenience.

## Boundaries Protect Complexity

Advanced behavior should be isolated behind clear boundaries: module targets,
platform handlers, repositories, plugin-like seams, or `Boundary` scopes.

The point of a plugin or handler seam is not extensibility for its own sake. It
is to prevent platform, media, game, or experimental behavior from weakening the
core model. A seam is good when it keeps ownership, lifecycle, and dirty/apply
contracts inspectable.

A `Boundary` is both a compartment and a diagnostic boundary. It localizes
ownership, lifecycle, update routing, composition policy, and platform
projection so complexity has a place to live and a place to be inspected.

A `Boundary` should also make independently owned application regions
composable. Different composition, update, or projection strategies may live
inside different boundaries, but they should meet the outside world through the
same state, tracker, dirty-result, and lifecycle contracts. This lets nested or
sibling regions cooperate without forcing a parent to inspect a child's
internals.

A `Boundary` protects complexity; it should not become the complexity. This is
not only a Boundary rule. Any long-lived object can become an accidental
controller if it collects every property, operation, platform detail, and
business rule that passes nearby. A Boundary is an owner, lifecycle scope,
tracker, composition boundary, and diagnostic surface; other framework objects
have their own narrower roles. When any object begins to grow a broad set of
mutable fields plus general-purpose methods, split facts from operations:
completed state belongs in small value objects or explicit owner state, while
behavior belongs in Flow, actions, policies, repositories, domain objects, or
platform projection seams.

## No Black Boxes

Loka should avoid black boxes. Powerful features may hide routine mechanics, but
they must not hide ownership, lifecycle, update flow, or the path to customize
behavior.

When a feature becomes complex, prefer a clear compartment with named extension
points over a sealed subsystem. Users and maintainers should be able to inspect
what owns the work, replace the platform projection, override the policy, or
move the behavior behind a different `Boundary` or module without breaking the
rest of the model.

Abstractions are welcome when they reduce accidental decisions. They are not
welcome when they make the framework impossible to reason about.

## Escape Hatches Must Look Like Escape Hatches

Loka should provide ways to cross boundaries, integrate unusual platform
behavior, and take manual control when a real application needs it. Those paths
must not look like the normal path.

If an API bypasses ordinary ownership, lifecycle, state creation, dirty routing,
or platform projection rules, its name and type shape should make that visible.
The `dangerously*` naming style is intentional: it allows power without
normalizing risk.

The best API is one people can usually use by feel because the concept set is
small and the names are precise. Documentation should explain and confirm the
model, not compensate for vague names or hidden contracts.

## Test What Can Be Known

Loka should prefer unit tests wherever behavior can be verified without a
specific operating system or machine. Core ownership, state routing, composition,
diffing, formatting, resource policy, and Flow behavior should be covered by
small tests that run often.

Platform and hardware behavior still matters. When behavior depends on an OS,
native toolkit, emulator, or real machine, Loka should move toward Flow-based
capture and automated scenario tests that can record, compare, and detect
regressions across environments.

The testing goal is not only correctness at one moment. It is to make future
porting, optimization, and platform-specific work safer by preserving evidence
of how the system behaves.

## Explicit Ownership

A good Loka abstraction should make it possible to answer:

- Who owns this state?
- Which lifecycle cleans it up?
- Which boundary sees changes?
- What becomes dirty when it changes?
- Which platform layer is allowed to reflect it?

If those answers are not visible in the type or API shape, the abstraction is
not ready to become a framework-facing surface.

## Prefer Immutable Facts

Completed facts should prefer immutable shapes. A snapshot, props object, apply
plan, analysis result, or other completed value should normally become read-only
once it has been built. Mutation belongs in a named owner, builder, transaction,
or local construction phase.

This is not immutability for fashion. It limits the number of places where a
value can change, makes Flow and update routing easier to debug, and prevents a
framework object from slowly becoming a shared mutable bag. When performance on
68030-era or other supported targets makes copying too expensive, Loka may use
explicit mutable storage, reusable buffers, or owner-local caches. The mutable
section should still be narrow and named, and immutable subsections should be
split into separate value types when that keeps the change surface smaller.

A growing set of properties plus general methods is often a sign that a class is
becoming an accidental owner. Prefer stack-local construction, explicit input
arguments, diff/result objects, builders, and small state machines over
permanently adding fields to a long-lived object. Paying a small cost to keep
facts immutable-like and lifecycle-aware is usually better than making cleanup,
ownership, and update routing implicit.

## State Ownership Is Explicit, Not Ambient

Loka should not assume that every `Node` is a state owner. A `Node` may express
meaning, receive props, compose children, draw, handle events, or declare local
state handles, but those capabilities do not automatically make it a lifecycle
owner.

Making all nodes own state would be convenient at first, but it would scatter
lifetime, tracking, cleanup, and update roots across the whole tree. That makes
applications harder to optimize and harder to inspect, especially on retro
targets where every hidden tracker, list, and allocation matters.

The default path should be that ordinary node-local state is backed by the
attached `Boundary` owner. Shorter or more specific lifetimes should be explicit:
a scope, logic/model node, resource owner, or boundary-inner owner should say in
its type shape that it owns state. Loka should make it easy to create such
owners when they are meaningful, but it should not create them everywhere by
default.

## Read And Write State Are Different

State access should communicate authority. If a prop or API only needs to read
a live value, it should accept `State<T>*`. Requiring `MutableState<T>*` for a
read-only input gives the callee apparent write authority it does not need.

Use `MutableState<T>*` when the component, platform bridge, or framework feature
is expected to write a result back to the caller, such as text input, selection
changes, file dialog results, or window state. Use `State<T>*` for borrowed live
inputs such as labels, enabled flags, layout alignment, padding, font size, or
other values that affect rendering or layout but are not owned or mutated by
the receiving node.

This distinction keeps application code meaningful: a state binding should show
whether data flows into a node, out of a node, or both. It also keeps ownership
and debugging clearer on targets without ARC, tracing GC, or pervasive smart
pointers.

## Boundary-Centered Memory

Memory and resource ownership should be organized around `Boundary` whenever
possible. A `Boundary` is not only an update scope; it is the place where state,
trackers, flows, composition snapshots, and local resources become accountable.

When data is shared across boundaries, the owner should still be obvious. A
pointer, handle, `NodeState<T>`, `Managed<T>`, repository, or future resource
facade should make it inspectable which boundary or higher-level owner keeps the
payload alive. That makes leaks and premature cleanup easier to debug: the
question becomes "which owner is still holding this?" rather than "which hidden
runtime retained this?"

Loka cannot assume ARC, tracing GC, exceptions, or modern smart-pointer-heavy
code on every target. That is not a weakness. It is a reason to make ownership
visible, centralized, and lifecycle-aware instead of accidental.

## Composition Strategy

`StdComposition` is the default scene composition model because Loka has already
validated that `State` + `StateTracker` can provide strong reactive behavior
with much less machinery than full structural reconciliation.

Small implementation should not mean small expressive power. The standard
composition path intentionally avoids implicit whole-tree reconciliation as the
default model. It favors explicit ownership, visible lifecycle phases, and
localized update paths that can be understood without reverse-engineering a
hidden matching algorithm.

`attachNode`, `composeNode`, and `detachNode` exist so composition can be
recomposable without collapsing setup, structural declaration, and teardown into
one ambiguous operation. A node should be able to say when it enters a
composition scope, what children it declares, and when its local resources or
bindings leave that scope.

More dynamic composition strategies are not forbidden. They can exist as
separate concrete strategies behind a `Boundary` when their identity, lifetime,
state preservation, and platform-context rules are explicit. The default path
should remain simple, inspectable, and friendly to retro targets.

Menu composition and future media/game composition may use different strategies
when their lifecycle and platform constraints justify it. Those differences
should be explicit, not accidental.

Loka should not add another scene composition algorithm only because it is
theoretically possible. New strategies should be justified by a domain that
needs different timing, redraw, reuse, or resource rules, such as real-time
surfaces, animation, games, or video tools. Until such pressure is concrete,
bugs and missing behavior should be fixed in the standard path rather than
splitting the model prematurely.

Performance pressure alone should usually be handled as a target profile before
it becomes a new application model. The standard composition path should remain
modern, practical, and expressive. Extremely constrained targets may justify a
separate 68k-focused Boundary/profile that sacrifices convenience, diagnostics,
or generality for smaller memory, code size, and runtime overhead, but it should
still honor the same ownership, state/tracker, dirty-result, and lifecycle
contracts so it can coexist with ordinary Boundaries.

## Platform Native, Core Neutral

Platform layers should follow native conventions. Core and app-facing DSL should
stay neutral. Use names like `controlTag` rather than `toolboxControl` unless an
API is intentionally platform-specific.

Platform conventions such as macOS application menus or Win32 window-attached
menus should become explicit policies instead of implicit behavior scattered
through platform code.

## Retro Constraints As Design Pressure

C++98 support, no exceptions, classic hardware, and disabled RTTI are not only
compatibility constraints. They are useful pressure to keep abstractions small,
predictable, and cheap to inspect.

When modern convenience conflicts with those properties, prefer the design that
keeps ownership and update flow understandable.

## Modern Type Safety Without Modern Assumptions

Loka should pursue modern type safety as far as each target allows. The fact
that the portable core must remain C++98-friendly should not excuse ambiguous
runtime contracts, untyped channels, or framework APIs that accept almost
anything and fail later.

Prefer compile-time structure over runtime discovery: typed props, explicit
owner handles, narrow result types, template constraints, TypeTag checks,
`asXxx()` accessors instead of RTTI, and small value/result objects that make
valid usage visible from code shape. When a modern compiler offers stronger
checks such as `static_assert`, Loka should benefit automatically. When a
classic compiler cannot provide the same check, the API should still be shaped
so misuse is difficult, reviewable, and easy to test.

The goal is not to imitate modern language features for their own sake. The
goal is to give application authors the same confidence: if code compiles, its
ownership, state direction, boundary crossing, and platform capability
assumptions should already be much narrower than an untyped runtime framework
would allow.

## Modern Code Is Welcome At The Edges

Loka's repository code keeps a C++98-friendly baseline so the same framework
ideas can survive on retro targets. That baseline should not be mistaken for a
restriction on every application built with Loka.

Applications targeting modern systems may integrate C++11 and later, Swift,
Rust, Unity, platform SDKs, or other tools when that is the right way to deliver
the product. Those integrations should live at clear app, repository, platform,
module, or plugin boundaries so they add capability without making the portable
Loka core depend on modern-only assumptions.

## Do Not Surrender To The Environment

Loka should not treat language, compiler, platform, or hardware limits as a
reason to give up on good software design. Constraints are real, but they should
be converted into better structure whenever possible.

If C++98 lacks a modern feature, look for a static, explicit, inspectable
alternative before accepting a weaker runtime contract. If a platform API is
awkward, isolate it behind a meaningful projection instead of leaking its
awkwardness into application code. If a retro target is slow, use that pressure
to make ownership, allocation, and update flow simpler rather than abandoning
safety.

This does not mean cleverness for its own sake. It means being unwilling to
lower the design bar just because the environment is old, small, or inconvenient.
