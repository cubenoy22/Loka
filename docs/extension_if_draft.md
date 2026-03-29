# Loka Extension I/F Draft

This note sketches a low-cost extension compatibility model for Loka.

The current goal is deliberately narrow:

- provide a build-time compatibility contract
- avoid runtime registration overhead unless needed later
- avoid forcing package-manager-level dependency resolution early
- keep Classic/68k-friendly constraints in mind

This is a draft. It defines direction and terminology, not a finalized public
API.

---

## 1. Goal

Loka should be able to say:

- this extension targets a known Loka Extension I/F generation
- incompatible generations fail at build time
- compatible generations require no runtime negotiation

The model should stay light enough for:

- C++98
- no exceptions
- Classic/68k-sensitive builds

---

## 2. Compatibility Model

The proposed model uses:

- one Loka version
- one extension-interface version family
- optional static capability tags

Important rule:

- ecosystem packages should primarily depend on Loka itself
- package-to-package dependency chains should be avoided for now

That means the compatibility anchor is:

- `LOKA_VERSION`
- `LokaExtensionEntryV1Tag` / `V2Tag` / ...

not a deep plugin dependency graph.

---

## 3. Why Tags First

For the current phase, a tag-based design is preferable to runtime virtual
extension interfaces.

Reasons:

- compile-time compatibility checks are enough for the first iteration
- no extra runtime dispatch or registration cost
- no ABI promises are implied accidentally
- it fits existing `TypeTag`-style checks already used in Loka

The intention is:

- mark extension-facing seams statically
- reject mismatched versions at build time
- add runtime/plugin machinery only if a real use case appears

---

## 4. Proposed Shape

### 4.1 Entry tags

```cpp
struct LokaExtensionEntryV1Tag {};
struct LokaExtensionEntryV2Tag {};
```

An extension-facing type opts in explicitly:

```cpp
struct MyExtensionHost
{
  typedef LokaExtensionEntryV1Tag ExtensionEntryTag;
};
```

Registration or wiring sites then statically require the expected tag.

Example direction:

```cpp
template<class TExpectedTag, class T>
struct LokaRequireExtensionEntryTag;
```

If a `V1` extension is used at a `V2` entry point, the build fails.

### 4.2 Optional capability/system tags

Version tags alone are intentionally coarse. A second layer can describe which
system boundary an extension touches.

Possible shape:

```cpp
struct LokaExtensionStateSystemTag {};
struct LokaExtensionSceneSystemTag {};
struct LokaExtensionWindowSystemTag {};
struct LokaExtensionNodeSystemTag {};
```

Then an extension-facing type can declare both:

```cpp
struct MySceneHook
{
  typedef LokaExtensionEntryV1Tag ExtensionEntryTag;
  typedef LokaExtensionSceneSystemTag ExtensionSystemTag;
};
```

This stays static and does not require runtime metadata.

---

## 5. Recommended Tag Granularity

The important design choice is:

- do not tag every internal class as stable
- tag only extension-facing system boundaries

That means the preferred unit is not:

- `Boundary` as public stable contract
- `StateTracker` as public stable contract

but rather:

- State system
- Scene system
- Window system
- Node system

### Recommended first-pass system tags

#### `LokaExtensionStateSystemTag`

Covers:

- `State`
- `MutableState`
- `DerivedState`
- `StateTracker`
- state notification / dependency boundaries

Use when an extension interacts with Loka's reactive core.

#### `LokaExtensionSceneSystemTag`

Covers:

- `Scene`
- `SceneDirector`
- boundary update/apply semantics
- scene invalidation / scheduling boundaries

Use when an extension depends on UI update-cycle behavior.

#### `LokaExtensionWindowSystemTag`

Covers:

- `Window`
- `App`
- window lifecycle / visibility / menu / close-request boundaries

Use when an extension integrates at the application/window shell layer.

#### `LokaExtensionNodeSystemTag`

Covers:

- `Node`
- node definitions
- DSL-facing node integration points
- node/context specialization seams

Use when an extension introduces custom node/control/widget behavior.

---

## 6. What Not To Mark Yet

The following are important internally, but should not automatically be treated
as stable extension entry points.

### `Boundary`

Reason:

- still under active architectural refinement
- owns many internal lifecycle and apply details
- too easy to over-promise stability

If external code needs boundary-like access later, prefer a narrower facade such
as a dedicated boundary-host interface, not `Boundary` itself as the contract.

### `StateTracker`

Reason:

- important, but still implementation-sensitive
- extensions should more often care about the state system boundary than about
  `StateTracker` internals specifically

So `StateTracker` belongs under `LokaExtensionStateSystemTag`, not as its own
public extension family.

### `Scene`, `Window`, `Node`

These names are good mental anchors, but the public contract should still be the
system tag first. The actual classes may evolve as long as the system boundary
remains compatible.

---

## 7. Compatibility Policy Direction

A possible future rule:

- extension-facing registration points declare a required entry version
- optional system tags/capability tags refine what kind of extension is allowed
- older tags may be accepted only through explicit trait-based compatibility

---

## 7.5 Custom Node Registration Direction

The first practical extension seam in the current refactor line is custom node
registration.

The internal direction is:

- custom `Node` types identify themselves by `nodeTypeKey()`
- platform-specific node handlers are registered against that key
- built-in nodes use the same handler/registry path as external nodes

Minimal shape:

```cpp
class CustomNode : public loka::app::scene::Node
{
public:
  virtual const void *nodeTypeKey() const
  {
    return loka::app::scene::NodeTypeToken<CustomNode>();
  }
};

class CustomHandler : public loka::app::scene::IPlatformNodeHandler
{
public:
  virtual const void *nodeTypeKey() const
  {
    return loka::app::scene::NodeTypeToken<CustomNode>();
  }

  virtual loka::app::scene::NodeContext *ensureContext(
      loka::app::scene::Node *node,
      loka::app::scene::IPlatformController *controller,
      const loka::app::scene::LayoutState &state);
};
```

Registration direction:

```cpp
controller->registerNodeHandler(new CustomHandler());
```

This keeps the first seam narrow:

- no plugin loader is implied
- no ABI-stable binary contract is implied
- external code can still add node/control behavior without editing Loka core

The important architectural rule is:

- node selection stays in the registry/factory layer
- common node behavior may live in shared `NodeContext`/native-context helpers
- `PlatformController` should trend toward traversal/ownership only

---

## 7.6 Custom Layout Direction

The current container-layout refactor is intentionally internal-first.

Shared helpers now cover built-in retained containers such as:

- `Box`
- `ZStack`
- `Column`
- `Row`
- `Grid`

These helpers are not yet a public extension API. They exist to stabilize the
shape of layout inputs/outputs before committing to a public seam.

The intended progression is:

1. extract platform-independent container layout kernels into shared helpers
2. keep `PlatformController` responsible only for recursion callbacks,
   boundary-bounds application, and platform ownership hooks
3. identify the stable layout contract used by both built-in and custom nodes
4. expose registration only after that contract is demonstrated by built-in use

The likely long-term split is:

- leaf layout extension
  - intrinsic size / single-node placement
- container layout extension
  - child traversal / child-state distribution / aggregate result bounds

This distinction matters because container layout is not just another node
handler. It owns recursive child placement policy.

So the direction should not be:

- force container layout into the exact same seam as context creation

but rather:

- keep node-handler registration for context/native behavior
- add a separate layout registration seam once the shared container helper shape
  is stable

Possible future shape:

```cpp
class INodeLayoutHandler
{
public:
  virtual ~INodeLayoutHandler() {}
  virtual const void *nodeTypeKey() const = 0;
  virtual int layoutNode(
      loka::app::scene::Node *node,
      const LayoutState &state,
      ILayoutTraversal *traversal) = 0;
};
```

Where `ILayoutTraversal` would be a narrow callback/facade used to:

- lay out a child recursively
- query boundary application hooks if needed

The important restriction is:

- do not expose `PlatformController` itself as the public layout contract

The public seam should stay smaller and more stable than the full controller.

For now, the practical rule is:

- keep layout extension work internal until at least one more built-in
  container/layout family proves the contract

That is enough direction to guide current refactors without prematurely locking
the public API.

Example direction:

- `V1 + StateSystem` may be accepted by a future `V2` state host if the
  compatibility trait says so
- `V1 + NodeSystem` may be rejected at a `V2` node host if node semantics
  changed

This keeps compatibility explicit rather than implicit.

---

## 7.5 Custom Node Registration Direction

The current custom-node seam is intentionally small:

1. a custom `Node` overrides `nodeTypeKey()`
2. external code provides an `IPlatformNodeHandler`
3. the active platform controller accepts it through `registerNodeHandler(...)`

Minimal shape:

```cpp
class MyCustomNode : public loka::app::scene::Node
{
public:
  virtual const void *nodeTypeKey() const
  {
    return loka::app::scene::NodeTypeToken<MyCustomNode>();
  }
};

class MyCustomHandler : public loka::app::scene::IPlatformNodeHandler
{
public:
  virtual const void *nodeTypeKey() const
  {
    return loka::app::scene::NodeTypeToken<MyCustomNode>();
  }

  virtual loka::app::scene::NodeContext *ensureContext(
      loka::app::scene::Node *node,
      loka::app::scene::IPlatformController *controller,
      const loka::app::scene::LayoutState &state)
  {
    (void)controller;
    (void)state;
    if (!node->getContext())
    {
      node->setContext(new loka::app::scene::NodeContext(node));
    }
    return node->getContext();
  }
};
```

Then the application side registers it on the active controller:

```cpp
MyCustomHandler handler;
platformController->registerNodeHandler(&handler);
```

This is not the final external-extension story yet.
What it gives now is a concrete, test-covered path away from `NODE_KIND` /
`asXxxNode()` hardwiring toward registered per-node platform behavior.

---

## 8. Recommended First Step

The smallest useful starting point is:

1. define `LokaExtensionEntryV1Tag`
2. define the four system tags
   - `StateSystem`
   - `SceneSystem`
   - `WindowSystem`
   - `NodeSystem`
3. add compile-time helper traits/asserts only
4. do not add runtime plugin loading yet

That gives Loka:

- a clear compatibility vocabulary
- no runtime cost
- no ABI promise beyond the intended seam

---

## 9. Future Expansion

If the ecosystem grows, the model can evolve toward:

- `LokaAppExtensionEntryV1Tag`
- `LokaGameExtensionEntryV1Tag`
- renderer/media/backend-specific capability tags
- optional runtime registries

But those should come only after the static compatibility model proves useful.

For now, the best policy is:

- keep the core engine small
- keep ecosystem dependencies shallow
- use extension tags to define what is intentionally stable
