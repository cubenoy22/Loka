# Rules and Reactivity Draft

## Why this note exists

This note is broader than the current update-pipeline work.

The update-pipeline discussion revealed a larger Loka principle:

- reactive systems become hard to maintain when state changes can silently trigger unrelated state changes across component boundaries

This is a common weakness in modern declarative UI systems.

Loka should prefer explicit reactivity over hidden reactivity.

## Main idea

Prefer:

- explicit rules
- explicit flows
- explicit ownership
- explicit update cycles

Avoid:

- scattered cross-component state mutation
- hidden feedback paths
- "this state changed, then something somewhere changed another state"
- reactive behavior with unclear causality

## Problem pattern

The bad pattern is:

```text
component A changes state X
  -> indirectly triggers component B to change state Y
  -> indirectly triggers component C to change state Z
  -> eventually re-enters A or another unrelated area
```

This creates:

- hidden coupling
- hard-to-reason-about updates
- update loops
- difficult debugging
- poor team-scale maintainability

This is one of the most frustrating parts of current declarative frameworks.

## Loka direction

Loka should push reactive behavior toward named, explicit structures.

Examples:

- `Rule`
- `Flow`
- explicit event handlers
- explicit ownership boundaries

Instead of:

- implicit reactive chains inside random components

Prefer:

- "this rule observes A and updates B"
- "this flow reacts to event X and produces state Y"

## Rules over hidden effects

Responsive behavior, environment reaction, and passive synchronization should preferably be expressed as rules.

Examples:

- window width changes
- responsive mode switches
- selection synchronization
- layout-derived mode changes
- state updates triggered by environment signals

These should ideally live in one explicit place rather than being scattered through arbitrary component code.

## Flow as explicit orchestration

`Flow` is a good place for reactive orchestration because it makes causality more visible.

Rough idea:

```text
observe signal
  -> evaluate condition
  -> trigger named rule/flow
  -> update state or emit event
```

This is preferable to:

```text
component body / callback / side effect
  -> silently mutates other state
```

## Parent ownership

Loka should continue to prefer parent ownership of state and data.

Good default:

- parent owns child-facing state
- child may emit intent
- parent decides resulting mutation

Avoid:

- child effectively controlling parent state through hidden mutation chains

Cross-boundary coordination should remain explicit.

## Event-cycle thinking

Reactive work should preferably be understood in event cycles.

```text
signal(s) happen
  -> updates are collected
  -> rules/flows react
  -> logical UI settles
  -> platform apply happens
```

This is easier to reason about than arbitrary immediate mutation at many points in the tree.

## Responsive behavior

Responsive behavior is important, but it should still be explicit.

Preferred style:

- a named responsive rule
- a named flow
- a clearly owned state transition

Less preferred style:

- many unrelated components each mutating state based on local layout observations

## Loka principle

A rough principle:

> Strong reactivity is acceptable.
> Hidden causality is not.

Or more concretely:

- reactive behavior should be powerful
- but the path from cause to effect should remain visible in code and explainable in architecture

## Practical guidance

Prefer:

- named rules for passive/reactive behavior
- flow-driven orchestration
- parent-owned state transitions
- explicit boundary crossing
- cycle-based updates

Be careful with:

- layout -> state -> layout feedback
- unrelated state mutation across component boundaries
- component-local side effects that reach far beyond the component
- reactive designs that are convenient at first but impossible to trace later

## Current conclusion

This is not only an update-pipeline concern.

It is a broader Loka design principle:

- use explicit rules and flows to organize reactive behavior
- keep causality visible
- avoid framework-style hidden coupling where possible

This principle should inform:

- update architecture
- responsive design patterns
- flow design
- component ownership conventions
