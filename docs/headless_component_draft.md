# Headless Component Draft

## Why

Some resources are too easy to allocate casually in local variables or component
members:

- state-derived bindings
- stream/flow chains
- temporary helper objects whose lifetime should follow UI structure

Those resources should not float outside the composition model.

Loka already has a strong boundary-first ownership model. This draft adds a
second scoped placement for lifecycle-bound resources.

## Existing Scope

Boundary-scoped resources already have a natural home:

- `declareStates()`
- future boundary-owned stream/flow helpers
- explicit sharing via `Managed<T>` / facade when needed

These resources:

- belong to the owning `Boundary`
- may be shared across components inside the same boundary
- may outlive a specific `Show(...)` / conditional subtree
- should not disappear just because a child subtree is hidden

## New Scope

Some resources should instead follow a subtree:

- local derived values only needed while a conditional branch is alive
- temporary processing state for a specific composed region
- helper pipelines that should disappear when the subtree detaches

For those, a headless component/resource scope is a better fit.

## Goal

Introduce a composition-declared headless scope that:

- is declared from composition, not by casual local allocation
- belongs to a specific composed subtree
- is visible to the declaring component and its children
- is destroyed when that subtree detaches
- does not become a new cross-boundary mutation path

## Practical Rule

If a resource's lifetime is easy to get wrong, declare it through composition
first.

This applies especially to:

- derived state created from `stream().map(...)`
- flow chains
- helper bindings that may survive beyond the immediate compose call
- expressions that capture external backing storage

The goal is not perfection, but to eliminate the easiest crash-prone casual
paths.

## Non-Goal

This is not for cross-boundary sharing.

Cross-boundary sharing already has a model:

- parent-owned state
- `Managed<T>`
- facade / borrowed access

Headless scope is only about where lifecycle-bound data processing should live.

## Stable Source Rule

Derived bindings and expressions may only capture stable backing storage.

Allowed examples:

- boundary-owned state declared through composition
- component members with the same or longer lifetime than the derived binding
- explicit shared resources such as `Managed<T>`

Disallowed examples:

- local vectors built inside `composeNode()`
- temporary objects used as backing storage for `At(...)` or similar helpers
- ad hoc objects whose lifetime is shorter than the binding/resource using them

Immediate constant values are still fine. The restriction only applies to
backing storage that may be observed or referenced after the immediate call.

## Comparison

### Boundary-owned composition resource

Use when:

- multiple components inside one boundary need the same resource
- the resource should survive `Show(...)` / conditional subtree replacement
- the resource may later be exposed via facade or `Managed<T>`

Properties:

- boundary lifetime
- reusable inside the boundary
- stable across subtree detach/reattach

### Headless composition resource

Use when:

- the resource only matters for one composed region
- the resource should disappear with that subtree
- the resource should not accidentally become broadly shared

Properties:

- subtree lifetime
- visible to declaring component and descendants only
- detached with the subtree

## Possible Shape

This draft intentionally leaves naming open.

Possible API directions:

```cpp
c.headless().state(...)
c.headless().stream(...)
c.headless().flow(...)
```

or

```cpp
HeadlessDef(...)
```

or

```cpp
c.declareHeadless(...)
```

The key point is not the exact name, but the rule:

- if a resource's lifetime is easy to get wrong, declare it through composition
- then choose boundary scope or subtree/headless scope explicitly

## Why This Matters

This should reduce crash-prone casual code paths for both humans and AI:

- fewer ad hoc member objects whose lifetime is unclear
- fewer local helper objects that silently outlive their source assumptions
- easier review of ownership and teardown behavior

It also gives a clearer comparison point for future refactors:

- boundary-owned composition resource
- headless/subtree-owned composition resource

If one pattern starts to break down, the failure mode should be easier to see.

## First Experiment

The first prototype should stay small.

Suggested experiment:

- create a headless-scoped derived resource under a `Show(...)` branch
- confirm it is available while visible
- confirm it disappears when the branch detaches
- confirm no cross-boundary sharing is implied
- compare the resulting usage against a boundary-scoped composition resource

The first implementation does not need a brand-new primitive.

A nested boundary with a tiny or empty visual surface is enough to validate the
lifecycle rule:

- it already owns state safely
- it already detaches structurally with `Show(...)`
- it gives a direct comparison against normal boundary-scoped state

If that prototype feels too heavy or too visible in real DSL usage, it becomes a
stronger argument for a dedicated headless/subtree-owned composition surface.

Current comparison result:

- nested boundary is a good control case
- it hides correctly under `Show(...)`
- but its owned state survives hide/show
- so it behaves like boundary scope, not subtree/headless scope

That is useful: it gives a concrete baseline that demonstrates why a dedicated
headless/subtree-owned scope would still be valuable.

Current prototype status:

- a dedicated headless owner can be attached as a subtree-local resource owner
- hiding the subtree destroys that owned state with the subtree
- re-show hydration is not solved yet and remains an open implementation issue

Current implementation note:

- `ConditionalNode` can replace branch children live
- plain/static children are fine because they do not need an attach compose step
- a non-boundary composable headless owner does need attach compose
- on re-show, that attach path is not currently re-entered for the new branch child

So the remaining issue is not headless ownership by itself. It is the missing
"new composable child attach" path for live conditional branch replacement.

## Open Questions

- Should headless resources be exposed through a tiny dedicated API, or through
  a generic `HeadlessProps`/vector-like container?
- Should headless scope own only state/stream/flow, or arbitrary helper objects?
- How much of the implementation should reuse `Managed<T>` internally?
- Should the public API emphasize `component/headless` naming or
  `subtree/lifecycle` naming?
