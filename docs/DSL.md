# Loka DSL Quick Intro

This is a short, memory-jogging intro to the Loka composition DSL and string usage.

## Composition Basics

Nodes are declared into a `NodeComposition` using chaining. Prefer DSL-style chaining and avoid local temporaries.

```cpp
#include "app/scene/StdComposition.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/controls/Button.hpp"

class DemoNode : public loka::app::scene::StdCompositionNodeFor<DemoNode>
{
public:
  typedef loka::app::scene::StdCompositionPropsFor<DemoNode> PropsType;
  DemoNode(const PropsType &p) : loka::app::scene::StdCompositionNodeFor<DemoNode>(p) {}

  virtual void composeNode(loka::app::scene::NodeComposition &c)
  {
    using namespace loka::app;
    c.declare(VStack()
              << Text("Hello")
              << Button("OK"));
  }
};
```

## Boundary + StdComposition

Boundaries own composition/state. Prefer `StdComposition` and keep conditional/repeated structure in the same model.

Loka is declarative in syntax, but it is not tightly coupled to a single composition strategy.
Each `Boundary` can own its own composition/reuse/redraw policy, and boundaries can be nested, so different parts
of a UI may use different strategies.

The currently provided `StdComposition` is a fine-grained reactive composition model: it builds the node structure
once, binds to state during compose, and then reflects updates through those bindings without rebuilding the
composition tree.

Recompose-capable composition is not impossible in Loka. It can be introduced as another concrete composition
strategy attached to a `Boundary`, making the subtree below that boundary recompose-capable. Loka currently ships
only `StdComposition` because it already provides enough expressive power, while keeping the implementation,
reuse behavior, and redraw strategy much simpler and cheaper.

```cpp
#include "app/scene/StdComposition.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/Text.hpp"

class StaticPanel : public loka::app::scene::StdCompositionNodeFor<StaticPanel>
{
public:
  typedef loka::app::scene::StdCompositionPropsFor<StaticPanel> PropsType;
  StaticPanel(const PropsType &p)
      : loka::app::scene::StdCompositionNodeFor<StaticPanel>(p) {}

  virtual void composeNode(loka::app::scene::NodeComposition &c)
  {
    using namespace loka::app;
    c.declare(VStack() << Text("Static content"));
  }
};
```

Use `Boundary()` (alias) when you need a nested owner for state or recomposition.

```cpp
using namespace loka::app::scene;
using namespace loka::app;

c.declare(VStack()
          << Boundary<StaticPanel>());
```

Notes:
- If a Scene root is non-boundary, it is wrapped by `RootBoundaryWrapper`.

## App Composition (WindowDef)

App-level composition declares windows and scenes.

```cpp
#include "app/AppComposition.hpp"
#include "app/WindowDefinition.hpp"
#include "app/nodes/Text.hpp"

class MyAppConfig : public AppConfigurable
{
public:
  explicit MyAppConfig(PlatformContext *ctx) : AppConfigurable(ctx) {}

  virtual void compose(AppComposition &c)
  {
    c << WindowDef(WindowProps()
                       .frame(40, 40, 320, 240)
                       .scene(loka::app::scene::NodeDefinition<MyProps, MyNode>())
                       .title("LokaApp")
                       .visible(true));
  }
};
```

## Fragment (F) + NodeComposition Helpers

`Fragment` (`F()`) is a lightweight, nestable container when you just want to group children without a visual node.

```cpp
#include "app/nodes/nestable/Fragment.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/Text.hpp"

using namespace loka::app;

c.declare(VStack()
          << F()
                 << Text("Line A")
                 << Text("Line B"));
```

`NodeComposition::group()` lets you store a definition in the composition arena and reuse it.

```cpp
using namespace loka::app;

Text *label = c.group(Text("Reusable"));
c.declare(VStack() << *label << *label);
```

Use scopes when you need to temporarily change the active parent or capture the current composition.

```cpp
using namespace loka::app;
using namespace loka::app::scene;

VStack &root = c.declare(VStack());
NodeComposition::ParentScope scope(c, root);
c.declare(Text("Child under root"));
```

`CompositionScope` installs the composition as current so helper APIs can access it.

```cpp
using namespace loka::app;
using namespace loka::app::scene;

NodeComposition::CompositionScope scope(c);
c.declare(Text("Composed while current() is set"));
```

## Ownership + Arena

`NodeComposition` clones definitions into an arena and tracks cleanup via hooks.
This makes `c.declare(...)` and `c.group(...)` safe for temporary DSL objects.

- Definitions passed into `declare`/`group` are cloned and owned by the arena.
- Cleanup hooks are installed so the arena can detect external destruction.
- Avoid manual `delete` on returned pointers; pass them into `operator<<`.

## Conditional

Use `NodeComposition::showIf` to include optional content inside `StdComposition`.

```cpp
using namespace loka::app;
using namespace loka::app::scene;

class ToggleNode : public StdCompositionNodeFor<ToggleNode>
{
public:
  typedef StdCompositionPropsFor<ToggleNode> PropsType;

  ToggleNode(const PropsType &p)
      : StdCompositionNodeFor<ToggleNode>(p),
        flag_()
  {
    this->state(this->flag_, false);
  }

  virtual void composeNode(NodeComposition &c)
  {
    c.declare(VStack()
              << (Show(*this->flag_.state()) << Text("On")));
  }

private:
  NodeState<bool> flag_;
};
```

This keeps the branch in the same `StdComposition` model. If the condition is
false, `Empty` is used.

## Stream Helpers

`loka::Vector` can be mapped/filtered/sorted using `Stream`.

```cpp
#include "loka/dsl/Stream.hpp"
#include "loka/dsl/Expr.hpp"

struct Item
{
  typedef loka::dsl::SlotProxyBase<Item> Slot;
  int value;
  explicit Item(int v) : value(v) {}
};

loka::Vector<Item> items;
items.push_back(Item(1));
items.push_back(Item(2));

loka::dsl::Stream<Item> s(items);
loka::Vector<int> values = s.map<int>(s.slot.member<int, &Item::value>() + loka::dsl::Const(1));
```

## State + Events

Use `EmitterState` for events and `NodeState<T>` for Node-local values. Register Node-local state with `this->state(...)` so it is attached to the active Boundary owner. Mutating state must be wrapped in a `loka::core::StateTrackerGuard`.

```cpp
#include "app/scene/NodeState.hpp"
#include "app/scene/StdComposition.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"
#include "loka/core/String.hpp"

class DemoNode : public loka::app::scene::StdCompositionNodeFor<DemoNode>
{
public:
  typedef loka::app::scene::StdCompositionPropsFor<DemoNode> PropsType;

  DemoNode(const PropsType &p)
      : loka::app::scene::StdCompositionNodeFor<DemoNode>(p),
        count_(),
        clickEvent_()
  {
    this->state(this->count_, 0);
  }

  void onClick()
  {
    loka::core::StateTrackerGuard guard(this->tracker());
    this->count_.set(this->count_.get() + 1, true);
  }

  loka::app::scene::NodeState<int> count_;
  loka::core::EmitterState clickEvent_;
};
```

Prefer `deferBind` for UI reflection/lazy updates. Use `bind` only when immediate recompute is required.

## String Usage

`loka::core::String` is the primary string type for UI text.

```cpp
#include "loka/core/String.hpp"

loka::core::String label = loka::core::String::Literal("Score: ");
label = label + loka::core::String::FromInt(42);
```

For constant text, `Text("Hello")` is fine; it uses `String::Literal` internally. For dynamic text, keep a `State<loka::core::String>` and bind it.

## Notes

- C++98 only.
- No RTTI in DSL/scene code; use `asXxx()` or `NodeKind` checks.
- In Classic builds, keep props stable and avoid transient data.
