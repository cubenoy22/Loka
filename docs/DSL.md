# Loka DSL Quick Intro

This is a short, memory-jogging intro to the Loka composition DSL and string usage.

## Composition Basics

Nodes are declared into a `NodeComposition` using chaining. Prefer DSL-style chaining and avoid local temporaries.

```cpp
#include "app/scene/node/StaticComposition.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "app/Button.hpp"

class DemoNode : public loka::app::scene::StaticCompositionNodeFor<DemoNode>
{
public:
  typedef loka::app::scene::StaticCompositionPropsFor<DemoNode> PropsType;
  DemoNode(const PropsType &p) : loka::app::scene::StaticCompositionNodeFor<DemoNode>(p) {}

  virtual void composeNode(loka::app::scene::NodeComposition &c)
  {
    using namespace loka::app;
    c.declare(VStack()
              << Text("Hello")
              << Button("OK"));
  }
};
```

## Boundary + Static Composition

Boundaries own composition/state. Prefer static composition and keep conditional/repeated structure in the same model.

```cpp
#include "app/scene/node/StaticComposition.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"

class StaticPanel : public loka::app::scene::StaticCompositionNodeFor<StaticPanel>
{
public:
  typedef loka::app::scene::StaticCompositionPropsFor<StaticPanel> PropsType;
  StaticPanel(const PropsType &p)
      : loka::app::scene::StaticCompositionNodeFor<StaticPanel>(p) {}

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
- `Group` nodes are non-boundary and use the nearest Boundary as `stateOwner`.
- If a Scene root is non-boundary, it is wrapped by `RootBoundaryWrapper`.

## App Composition (WindowDef)

App-level composition declares windows and scenes.

```cpp
#include "app/AppComposition.hpp"
#include "app/WindowDefinition.hpp"
#include "app/Text.hpp"

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
#include "app/Fragment.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"

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

`Group` is a non-boundary composable node. Use it when you want compose-time grouping
without owning state.

```cpp
#include "app/scene/node/Group.hpp"
#include "app/Text.hpp"

using namespace loka::app::scene;
using namespace loka::app;

class MyGroup : public GroupNodeBase<GroupPropsFor<MyGroup> >
{
public:
  typedef GroupPropsFor<MyGroup> PropsType;
  MyGroup(const PropsType &p) : GroupNodeBase<GroupPropsFor<MyGroup> >(p) {}

  virtual void composeNode(NodeComposition &c)
  {
    c.declare(Text("Grouped content"));
  }
};
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

Use `NodeComposition::showIf` to include optional content inside static composition.

```cpp
using namespace loka::app;
using namespace loka::app::scene;

BoundState<bool> flag;
c.declareStates().state(flag, false);

c.declare(VStack()
          << c.showIf(*flag.state(), Text("On")));
```

This keeps the branch in the same static composition model. If the condition is
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

Use `EmitterState` for events and `MutableState<T>` for values. Mutating state must be wrapped in a `loka::core::StateTrackerGuard`.

```cpp
#include "loka/core/State.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"
#include "loka/core/String.hpp"

class DemoComponent
{
public:
  DemoComponent()
      : count_(),
        clickEvent_()
  {
  }

  void attach(loka::app::scene::NodeComposition &c)
  {
    c.declareStates().state(count_, 0);
  }

  void onClick()
  {
    loka::core::StateTrackerGuard guard(&this->tracker_);
    this->count_.set(this->count_.get() + 1, true);
  }

  loka::core::MutableState<int> count_;
  loka::core::EmitterState clickEvent_;
  loka::core::PushStateTracker tracker_;
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
