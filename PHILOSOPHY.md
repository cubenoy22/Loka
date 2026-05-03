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

## Meaningful Code

Application-facing code should express intent, ownership, state flow, and
composition. It should not force users to think in platform IDs, encoding
buffers, native handles, or other incidental mechanics unless they are explicitly
working at that layer.

When an API name can reasonably be read in more than one way, tighten the name
or split the concept. Framework code should not depend on vibes.

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

React-style recomposition is not forbidden. It can exist as another concrete
composition strategy behind a `Boundary` when justified. The default path should
remain simple, inspectable, and friendly to retro targets.

Menu composition and future media/game composition may use different strategies
when their lifecycle and platform constraints justify it. Those differences
should be explicit, not accidental.

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
