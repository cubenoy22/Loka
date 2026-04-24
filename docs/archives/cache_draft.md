# Cache Draft

## Goal

Define a cache direction that works across:

- Classic/68k constrained paths
- PowerPC-era larger applications
- modern desktop targets
- future game-style workloads

The goal is to keep the outer contract stable while allowing cache policy and storage backends to vary by target and use case.

## Core Direction

Loka should not hard-code one global cache strategy.

Instead:

- outer cache contract stays explicit and small
- eviction / priority / preload policy remains replaceable
- platform backends may use native facilities where appropriate
- build-time prepared cache metadata should remain possible later

## Why A Generic `INodeCache` Is Too Narrow

Node retention is only one cache use case.

Other likely cacheable domains include:

- images
- fonts / glyphs
- menu metadata
- parsed immutable assets
- future game resources

A node-only cache interface would not scale well.

## Why A Single Huge `ICache` Is Too Broad

A fully generic catch-all cache tends to blur responsibilities:

- key policy
- lifecycle policy
- trim semantics
- ownership expectations

That makes it harder to keep behavior predictable on constrained targets.

## Preferred Layering

### 1. Core store contract

A small generic storage contract should sit at the bottom.

Possible direction:

- `ICacheStore<Key, Value>`
- `find`
- `retain`
- `release`
- `trim`

This is the low-level storage and lookup seam.

### 2. Policy contract

A separate policy layer should decide:

- priority updates
- scoring
- eviction ordering
- trim targets
- preload ranking

Possible direction:

- `ICachePolicy`

This is intentionally similar in spirit to `StateTracker`: a stable outer contract with replaceable policy.

### 3. Domain-specific facades

On top of the store/policy layer, Loka can expose narrower domain-specific wrappers, for example:

- `ImageCache`
- `NodeRetainCache`
- `MenuCache`
- `GlyphCache`

These facades keep the call sites explicit without forcing each domain to invent a completely separate cache substrate.

## Ownership Rule

Cache design should follow the same ownership rule as the rest of Loka:

- follow gravity
- default ownership stays with the parent/owner
- shared retention is explicit
- immutable/shared resources are the main candidates for broad cache reuse

Caches should not become a hidden substitute for ordinary ownership.

## Target-Specific Strategy

Different targets want different trade-offs.

### Classic / 68k

Prefer:

- tiny memory footprint
- simple lookup
- explicit limits
- cache disabled entirely when not worth the cost

### PowerPC / G3+

More memory can be spent to reduce CPU churn and UI latency.

Likely useful:

- stronger retain caches
- richer preloading
- larger fixed pools

### Modern Desktop

Implementation can be richer, but the outer contract should remain the same.

This is where more aggressive policy experiments are acceptable.

## Platform Backend Reuse

Platform-native cache primitives may be used behind the Loka cache contract.

Examples:

- macOS/iOS: `NSCache`
- modern desktop: custom map/LRU implementations
- Classic: tiny custom cache or no cache

Important rule:

- Loka owns the outer semantics
- platform facilities are optional backends, not the public model

## Runtime Policy vs Build-Time Preparation

Loka should leave room for both:

1. runtime policy
- usage-based retention
- distance/visibility-based ranking
- dynamic trim
- idle-time or low-priority sweep opportunities
- memory-pressure-triggered release

2. build-time preparation
- precomputed preload plans
- static asset grouping
- target-specific generated cache hints

The long-term direction should favor explicit build-time assistance where possible, rather than relying entirely on opaque runtime heuristics.

## Relationship To Game-Style Workloads

Game-style workloads may want policies such as:

- lower priority as distance increases
- aggressive retention for nearby or visible assets
- controlled background trimming

That argues for a replaceable policy layer, not for one fixed UI-only cache strategy.

## Immediate Non-Goals

Not part of the first cache design pass:

- global smart caching for everything
- automatic system-wide heuristics without explicit policy boundaries
- tightly coupling cache policy to a specific UI node implementation

## Immediate Next Step

If this direction is accepted, the next design pass should define:

- minimal `ICacheStore` responsibilities
- minimal `ICachePolicy` responsibilities
- which first domain should use the shared substrate
  - image cache
  - node retain cache
  - menu metadata cache
