#include "Tests.hpp"
#include <cassert>
#include <string>
#include "core/State.hpp"
#include "core/StateTracker.hpp"
#include "core/util/StateTrackerGuard.hpp"
#include "core/util/StateUtil.hpp"
#include "app/SceneManager2.hpp"
#include "core2/scene/NodeComposition.hpp"
#include "core2/scene/PlatformController.hpp"
#include "core2/scene/Scene.hpp"
#include "core2/scene/node/StaticComposition.hpp"
#include "core2/scene/node/StaticComposition.hpp"
#include "app/Box.hpp"
#include "app/Button.hpp"
#include "core/Managed.hpp"
#include "loka/core/String.hpp"
#include "loka/core/Value.hpp"
#include "loka/core/Vector.hpp"
#include "loka/platform/StringUTF8.hpp"
#include "loka/platform/String.hpp"
#include "loka/dsl/dsl.hpp"

// --- main.cppから移動したテスト関数をここに実装 ---

void testDependencyPropagationCases()
{
  printf("\n==== [testDependencyPropagationCases] start ====\n");
  printf("testDependencyPropagationCases\n");
  // --- 多段依存 A→B→C ---
  loka::core::MutableState<int> a;
  struct DerivedState : public loka::core::StateBase
  {
    loka::core::StateBase *dep;
    int value;
    DerivedState(loka::core::StateBase *d) : dep(d), value(0) {}
    virtual bool recompute()
    {
      loka::core::MutableState<int> *s = dynamic_cast<loka::core::MutableState<int> *>(dep);
      DerivedState *d2 = dynamic_cast<DerivedState *>(dep);
      int old = value;
      if (s)
        value = s->get() + 1;
      else if (d2)
        value = d2->value + 1;
      printf("recompute: state=%p, old=%d, new=%d\n", (void *)this, old, value);
      return value != old;
    }
    virtual std::vector<loka::core::StateBase *> getDependencyStates() const
    {
      return makeStateVector(dep, STATE_NULL);
    }
  };
  DerivedState b(&a);
  DerivedState c(&b);
  std::vector<loka::core::StateBase *> states = makeStateVector(&a, &b, &c, STATE_NULL);
  loka::core::PushStateTracker tracker(states);
  tracker.begin();
  a.set(10);
  tracker.end();
  assert(b.value == 11 && c.value == 12);
  tracker.begin();
  a.set(20);
  tracker.end();
  assert(b.value == 21 && c.value == 22);
  // --- 複数Derivedが1つのStateに依存 ---
  loka::core::MutableState<int> s;
  DerivedState d1(&s);
  DerivedState d2(&s);
  std::vector<loka::core::StateBase *> states2 = makeStateVector(&s, &d1, &d2, STATE_NULL);
  loka::core::PushStateTracker tracker2(states2);
  tracker2.begin();
  s.set(5);
  tracker2.end();
  printf("[TEST] 複数Derived依存: d1=%d, d2=%d (期待値: 6, 6)\n", d1.value, d2.value);
  assert(d1.value == 6 && d2.value == 6);
  // --- 依存先の更新時にDerivedが再評価されるか ---
  tracker2.begin();
  s.set(42);
  tracker2.end();
  printf("[TEST] 依存先更新: d1=%d, d2=%d (期待値: 43, 43)\n", d1.value, d2.value);
  assert(d1.value == 43 && d2.value == 43);
  // --- 依存していないDerivedが影響を受けない ---
  loka::core::MutableState<int> s2;
  DerivedState d3(&s2);
  std::vector<loka::core::StateBase *> states3 = makeStateVector(&s, &d1, &d2, &s2, &d3, STATE_NULL);
  loka::core::PushStateTracker tracker3(states3);
  tracker3.begin();
  s.set(100);
  s2.set(200);
  tracker3.end();
  printf("[TEST] 依存分離: d1=%d, d2=%d, d3=%d (期待値: 101, 101, 201)\n", d1.value, d2.value, d3.value);
  assert(d1.value == 101 && d2.value == 101);
  assert(d3.value == 201);
  printf("All dependency propagation tests passed.\n");
  printf("==== [testDependencyPropagationCases] end ====\n");
}

void testTrackerPropagation()
{
  printf("\n==== [testTrackerPropagation] start ====\n");
  loka::core::MutableState<int> s_int(10);
  struct DoublePropEval : public loka::core::DerivedState<int>::EvalFn
  {
    loka::core::MutableState<int> *s;
    DoublePropEval(loka::core::MutableState<int> *s_) : s(s_) {}
    int operator()() { return s->get() * 2; }
  };
  loka::core::DerivedState<int> *doubleProp = new loka::core::DerivedState<int>(makeStateVector(&s_int, STATE_NULL), new DoublePropEval(&s_int));
  printf("[test] s_int=%p, doubleProp=%p\n", (void *)&s_int, (void *)doubleProp);
  std::vector<loka::core::StateBase *> deps = doubleProp->getDependencyStates();
  for (size_t i = 0; i < deps.size(); ++i)
  {
    printf("[test] doubleProp.getDependencyStates()[%zu]=%p\n", i, (void *)deps[i]);
  }
  std::vector<loka::core::StateBase *> trackerStates = makeStateVector(&s_int, doubleProp, STATE_NULL);
  loka::core::PushStateTracker tracker(trackerStates);
  printf("[before begin] s_int=%d, doubleProp=%d\n", s_int.get(), doubleProp->get());
  tracker.begin();
  printf("[after begin] s_int=%d, doubleProp=%d\n", s_int.get(), doubleProp->get());
  s_int.set(21);
  printf("[after set] s_int=%d, doubleProp=%d\n", s_int.get(), doubleProp->get());
  tracker.end();
  printf("[after end] s_int=%d, doubleProp=%d\n", s_int.get(), doubleProp->get());

  assert(s_int.get() == 21);
  assert(doubleProp->get() == 42);
  printf("s_int: %d\n", s_int.get());
  printf("doubleProp: %d\n", doubleProp->get());
  printf("==== [testTrackerPropagation] end ====\n");
}

void testStateBatchOverflow()
{
  printf("\n==== [testStateBatchOverflow] start ====\n");
  struct DummyOwner : public loka::core::scene::IStateOwner
  {
    std::vector<loka::core::StateBase *> adopted;
    virtual void adoptState(loka::core::StateBase *state) { adoptStateUnchecked(state); }
    virtual void adoptStateUnchecked(loka::core::StateBase *state) { adopted.push_back(state); }
    virtual void reserveStates(size_t) {}
    virtual void reserveStateArena(size_t) {}
    virtual void *allocateStateMemory(size_t, size_t) { return 0; }
    virtual void registerStateMemory(loka::core::StateBase *, void (*)(loka::core::StateBase *)) {}
    virtual loka::core::StateTracker *tracker() { return 0; }
  };

  DummyOwner owner;
  loka::core::scene::BoundState<int> states[17];
  {
    loka::core::scene::NodeComposition::StateBatch batch(&owner);
    for (int i = 0; i < 17; ++i)
    {
      batch.state(states[i], i + 1);
    }
  }

  for (int i = 0; i < 17; ++i)
  {
    assert(states[i].isValid());
    assert(states[i].get() == i + 1);
  }
  assert(owner.adopted.size() == 17);
  printf("[test] state[0]=%d, state[15]=%d, state[16]=%d\n",
         states[0].get(), states[15].get(), states[16].get());
  printf("[test] adopted=%zu (expect 17)\n", owner.adopted.size());
  for (int i = 0; i < 17; ++i)
  {
    delete states[i].mutableState();
  }
  printf("==== [testStateBatchOverflow] end ====\n");
}

void testDeferredSideEffect()
{
  printf("\n==== [testDeferredSideEffect] start ====\n");
  loka::core::MutableState<int> s_int(5);
  struct DoublePropEval : public loka::core::DerivedState<int>::EvalFn
  {
    loka::core::MutableState<int> *s;
    DoublePropEval(loka::core::MutableState<int> *s_) : s(s_) {}
    int operator()() { return s->get() * 2; }
  };
  loka::core::DerivedState<int> *doubleProp = new loka::core::DerivedState<int>(makeStateVector(&s_int, STATE_NULL), new DoublePropEval(&s_int));
  std::vector<loka::core::StateBase *> trackerStatesDeferred = makeStateVector(&s_int, doubleProp, STATE_NULL);
  loka::core::PushStateTracker tracker(trackerStatesDeferred);
  struct DeferredCallback
  {
    static void onDeferred(void *)
    {
      printf("[deferred] UI redraw or log: commit completed!\n");
    }
  };
  tracker.defer(DeferredCallback::onDeferred, NULL);
  tracker.begin();
  s_int.set(7);
  tracker.end();
  assert(s_int.get() == 7);
  assert(doubleProp->get() == 14);
  printf("s_int: %d\n", s_int.get());
  printf("doubleProp: %d\n", doubleProp->get());
  printf("==== [testDeferredSideEffect] end ====\n");
}

void testTextInputOnChange()
{
  printf("\n==== [testTextInputOnChange] start ====\n");
  loka::core::MutableState<loka::core::String> name(loka::core::String::Literal(""));
  struct IsValidEval : public loka::core::DerivedState<bool>::EvalFn
  {
    loka::core::MutableState<loka::core::String> *n;
    IsValidEval(loka::core::MutableState<loka::core::String> *n_) : n(n_) {}
    bool operator()()
    {
      std::string utf8;
      if (!loka::platform::CollectUtf8(n->get(), utf8))
        return false;
      return utf8.size() >= 3;
    }
  };
  loka::core::DerivedState<bool> *isValid = new loka::core::DerivedState<bool>(makeStateVector(&name, STATE_NULL), new IsValidEval(&name));
  std::vector<loka::core::StateBase *> trackerStatesText = makeStateVector(&name, isValid, STATE_NULL);
  loka::core::PushStateTracker tracker(trackerStatesText);
  struct ValidCallback
  {
    static void onChange(void *userData)
    {
      loka::core::DerivedState<bool> *isValid = (loka::core::DerivedState<bool> *)userData;
      printf("[isValid] changed: %s\n", isValid->get() ? "OK" : "NG");
    }
  };
  printf("[TextInput] name.set(\"ab\")\n");
  tracker.begin();
  name.set(loka::core::String::Literal("ab"));
  tracker.end();
  assert(isValid->get() == false);
  printf("isValid: %s\n", isValid->get() ? "OK" : "NG");
  printf("[TextInput] name.set(\"senko\")\n");
  tracker.begin();
  name.set(loka::core::String::Literal("senko"));
  tracker.end();
  assert(isValid->get() == true);
  printf("isValid: %s\n", isValid->get() ? "OK" : "NG");
  printf("==== [testTextInputOnChange] end ====\n");
}

void testLokaDslStream()
{
  printf("\n==== [testLokaDslStream] start ====\n");
  struct Computer
  {
    loka::core::String name;
    loka::core::String manufacture;
    int year;

    struct Slot : public loka::dsl::SlotProxyBase<Computer>
    {
      typedef loka::dsl::Expr<loka::core::String, loka::dsl::MemberExpr<Computer, loka::core::String, &Computer::name> > NameExpr;
      typedef loka::dsl::Expr<loka::core::String, loka::dsl::MemberExpr<Computer, loka::core::String, &Computer::manufacture> > ManufactureExpr;
      typedef loka::dsl::Expr<int, loka::dsl::MemberExpr<Computer, int, &Computer::year> > YearExpr;

      NameExpr name;
      ManufactureExpr manufacture;
      YearExpr year;

      Slot(int index = 1) : SlotProxyBase<Computer>(index)
      {
        name = member<loka::core::String, &Computer::name>();
        manufacture = member<loka::core::String, &Computer::manufacture>();
        year = member<int, &Computer::year>();
      }
    };
  };

  loka::Vector<Computer> computers;
  {
    Computer c;
    c.name = loka::core::String::Literal("Macintosh");
    c.manufacture = loka::core::String::Literal("Apple");
    c.year = 1984;
    computers.push_back(c);
  }
  {
    Computer c;
    c.name = loka::core::String::Literal("PC/AT");
    c.manufacture = loka::core::String::Literal("IBM");
    c.year = 1984;
    computers.push_back(c);
  }
  {
    Computer c;
    c.name = loka::core::String::Literal("NeXTcube");
    c.manufacture = loka::core::String::Literal("NeXT");
    c.year = 1988;
    computers.push_back(c);
  }
  {
    Computer c;
    c.name = loka::core::String::Literal("Amiga");
    c.manufacture = loka::core::String::Literal("Commodore");
    c.year = 1985;
    computers.push_back(c);
  }

  {
    using namespace loka::dsl;
    loka::Vector<loka::core::String> labels =
        computers.stream().map<loka::core::String>(
            Const("Item ") + Index() + Const(": ") + computers.stream().slot.name);
    assert(labels.size() == 4);
    assert(labels[0].equals(loka::core::String::Literal("Item 0: Macintosh")));
    assert(labels[3].equals(loka::core::String::Literal("Item 3: Amiga")));

    loka::Vector<Computer> recent = computers.stream().filter(computers.stream().slot.year >= 1985);
    assert(recent.size() == 2);
    assert(recent[0].name.equals(loka::core::String::Literal("NeXTcube")));
    assert(recent[1].name.equals(loka::core::String::Literal("Amiga")));

    computers.stream().sort(computers.stream().slot.year > computers.stream().slot2.year);
    assert(computers[0].year == 1988);
    assert(computers[3].year == 1984);
  }

  printf("==== [testLokaDslStream] end ====\n");
}

void testBatchTransaction()
{
  printf("\n==== [testBatchTransaction] start ====\n");
  loka::core::MutableState<int> s1(1);
  struct SumPropEval : public loka::core::DerivedState<int>::EvalFn
  {
    loka::core::MutableState<int> *s;
    SumPropEval(loka::core::MutableState<int> *s_) : s(s_) {}
    int operator()() { return s->get() * 2; }
  };
  loka::core::DerivedState<int> *sumProp = new loka::core::DerivedState<int>(makeStateVector(&s1, STATE_NULL), new SumPropEval(&s1));
  loka::core::MutableState<int> s2(2);
  std::vector<loka::core::StateBase *> trackerStatesBatch = makeStateVector(&s1, &s2, sumProp, STATE_NULL);
  loka::core::PushStateTracker tracker(trackerStatesBatch);
  tracker.begin();
  s1.set(10);
  s2.set(20);
  tracker.end();
  assert(s1.get() == 10);
  assert(s2.get() == 20);
  assert(sumProp->get() == 20);
  printf("[Batch] s1=%d, s2=%d, sumProp=%d\n", s1.get(), s2.get(), sumProp->get());
  printf("==== [testBatchTransaction] end ====\n");
}

void testRAIITransaction()
{
  printf("\n==== [testRAIITransaction] start ====\n");
  loka::core::MutableState<int> s(0);
  struct DoublePropEval : public loka::core::DerivedState<int>::EvalFn
  {
    loka::core::MutableState<int> *s;
    DoublePropEval(loka::core::MutableState<int> *s_) : s(s_) {}
    int operator()() { return s->get() * 2; }
  };
  loka::core::DerivedState<int> *doubleProp = new loka::core::DerivedState<int>(makeStateVector(&s, STATE_NULL), new DoublePropEval(&s));
  std::vector<loka::core::StateBase *> trackerStatesRAII = makeStateVector(&s, doubleProp, STATE_NULL);
  loka::core::PushStateTracker tracker(trackerStatesRAII);
  {
    StateTrackerGuard _(&tracker);
    s.set(50);
  }
  assert(s.get() == 50);
  assert(doubleProp->get() == 100);
  printf("[RAII] s=%d, doubleProp=%d\n", s.get(), doubleProp->get());
  printf("==== [testRAIITransaction] end ====\n");
}

void testDerivedStruct()
{
  printf("\n==== [testDerivedStruct] start ====\n");
  struct FormInputs
  {
    loka::core::String name;
    loka::core::String email;
    int age;
    bool agree;
  };
  loka::core::MutableState<loka::core::String> name(loka::core::String::Literal(""));
  loka::core::MutableState<loka::core::String> email(loka::core::String::Literal(""));
  loka::core::MutableState<int> age(0);
  loka::core::MutableState<bool> agree(false);
  std::vector<loka::core::StateBase *> deps = makeStateVector(&name, &email, &age, &agree, STATE_NULL);
  struct IsValidEval : public loka::core::DerivedState<bool>::EvalFn
  {
    loka::core::MutableState<loka::core::String> *name;
    loka::core::MutableState<loka::core::String> *email;
    loka::core::MutableState<int> *age;
    loka::core::MutableState<bool> *agree;
    IsValidEval(loka::core::MutableState<loka::core::String> *n, loka::core::MutableState<loka::core::String> *e, loka::core::MutableState<int> *a, loka::core::MutableState<bool> *ag)
        : name(n), email(e), age(a), agree(ag) {}
    bool operator()()
    {
      const loka::core::String empty = loka::core::String::Literal("");
      return !name->get().equals(empty) && !email->get().equals(empty) && age->get() >= 18 && agree->get();
    }
  };
  loka::core::DerivedState<bool> *isValid = new loka::core::DerivedState<bool>(deps, new IsValidEval(&name, &email, &age, &agree));
  std::vector<loka::core::StateBase *> trackerDeps = makeStateVector(&name, &email, &age, &agree, isValid, STATE_NULL);
  loka::core::PushStateTracker tracker(trackerDeps);
  struct Callback
  {
    static void onChange(bool v, void *)
    {
      printf("[isValid] changed: %s\n", v ? "true" : "false");
    }
  };
  isValid->bind((loka::core::StateBase::OnChangeFn)Callback::onChange, isValid, false);
  tracker.begin();
  name.set(loka::core::String::Literal("senko"));
  email.set(loka::core::String::Literal(""));
  age.set(20);
  agree.set(true);
  tracker.end();
  assert(isValid->get() == false); // emailが空

  tracker.begin();
  email.set(loka::core::String::Literal("senko@ai"));
  age.set(15);
  tracker.end();
  assert(isValid->get() == false); // ageが未満

  tracker.begin();
  age.set(22);
  agree.set(false);
  tracker.end();
  assert(isValid->get() == false); // agreeがfalse

  tracker.begin();
  agree.set(true);
  tracker.end();
  assert(isValid->get() == true); // 全てOK
  printf("==== [testDerivedStruct] end ====\n");
}

void testSceneManagerTransaction()
{
  printf("\n==== [testSceneManagerTransaction/SceneManager2] start ====\n");
  SceneManager2 mgr;
  // ...必要に応じてテスト内容を追加...
  printf("--- [testSceneManagerTransaction/SceneManager2] end ---\n");
}

void testNodeCompositionTree()
{
  using namespace loka::core::scene;
  using namespace loka::app;

  loka::core::scene::NodeComposition composition;
  BoxProps boxProps;
  BoxDefinition box(boxProps);
  BoxDefinition &root = composition.declare(box);

  ButtonProps buttonProps;
  buttonProps.text("Hello");
  ButtonDefinition button(buttonProps);

  root << button;

  Node *tree = composition.createNodeTree();
  assert(tree != NULL);

  BoxNode *boxNode = dynamic_cast<BoxNode *>(tree);
  assert(boxNode != NULL);
  Node *child = boxNode->childrenHead();
  size_t childCount = boxNode->childrenCount();
  assert(child != NULL);
  assert(childCount == 1);
  ButtonNode *buttonNode = dynamic_cast<ButtonNode *>(child);
  assert(buttonNode != NULL);

  delete tree;
}

void testSceneMountLifecycle()
{
  using namespace loka::core::scene;

  class DummyPlatformController : public IPlatformController
  {
  public:
    DummyPlatformController() : lastMaterialized_(0), destroyed_(false) {}
    virtual void onChange(Node *rootNode, loka::core::scene::NodeDirtyFlags flags)
    {
      (void)flags;
      lastMaterialized_ = rootNode;
    }
    virtual void synchronize() {}
    virtual void destroy() { destroyed_ = true; }

    Node *lastMaterialized_;
    bool destroyed_;
  };

  Scene scene(StaticCompositionBoundary(StaticCompositionProps()));
  DummyPlatformController platform;
  scene.mount(&platform);
  scene.updateAttached(true);
  assert(platform.lastMaterialized_ != 0);
  assert(!platform.destroyed_);
  scene.updateAttached(false);
  assert(platform.destroyed_);
}

namespace
{
  int g_rootComposeCount = 0;
  int g_childComposeCount = 0;

  class ChildBoundaryNode;
  typedef loka::core::scene::BoundaryPropsFor<ChildBoundaryNode> ChildBoundaryProps;

  class ChildBoundaryNode : public loka::core::scene::BoundaryNodeFor<ChildBoundaryNode>
  {
  public:
    ChildBoundaryNode(const ChildBoundaryProps &p)
        : loka::core::scene::BoundaryNodeFor<ChildBoundaryNode>(ChildBoundaryProps(p)) {}

    virtual void composeNode(loka::core::scene::NodeComposition &c)
    {
      (void)c;
      ++g_childComposeCount;
    }
  };

  loka::core::scene::BoundaryDefinition<ChildBoundaryProps, ChildBoundaryNode> ChildBoundary()
  {
    return loka::core::scene::Boundary<ChildBoundaryNode>();
  }

  class RootBoundaryNode;
  typedef loka::core::scene::BoundaryPropsFor<RootBoundaryNode> RootBoundaryProps;

  class RootBoundaryNode : public loka::core::scene::BoundaryNodeFor<RootBoundaryNode>
  {
  public:
    RootBoundaryNode(const RootBoundaryProps &p)
        : loka::core::scene::BoundaryNodeFor<RootBoundaryNode>(RootBoundaryProps(p)) {}

    virtual void composeNode(loka::core::scene::NodeComposition &c)
    {
      ++g_rootComposeCount;
      c.declare(ChildBoundary());
    }
  };

  loka::core::scene::BoundaryDefinition<RootBoundaryProps, RootBoundaryNode> RootBoundary()
  {
    return loka::core::scene::Boundary<RootBoundaryNode>();
  }
}

void testSceneBoundaryNestedCompose()
{
  using loka::core::scene::IPlatformController;
  using loka::core::scene::Node;
  using loka::core::scene::Scene;

  class DummyPlatformController : public IPlatformController
  {
  public:
    DummyPlatformController() : lastMaterialized_(0), destroyed_(false) {}
    virtual void onChange(Node *rootNode, loka::core::scene::NodeDirtyFlags flags)
    {
      (void)flags;
      lastMaterialized_ = rootNode;
    }
    virtual void synchronize() {}
    virtual void destroy() { destroyed_ = true; }

    Node *lastMaterialized_;
    bool destroyed_;
  };

  g_rootComposeCount = 0;
  g_childComposeCount = 0;
  Scene scene(RootBoundary());
  DummyPlatformController platform;
  scene.mount(&platform);
  scene.updateAttached(true);
  assert(g_rootComposeCount == 1);
  assert(g_childComposeCount == 1);
  assert(platform.lastMaterialized_ != 0);
  assert(!platform.destroyed_);

  scene.updateAttached(false);
  assert(platform.destroyed_);

  scene.updateAttached(true);
  assert(g_rootComposeCount == 2);
  assert(g_childComposeCount == 2);

  // Unmount before stack-allocated platform is destroyed
  scene.unmount();
}

void testLokaCoreString()
{
  printf("\n==== [testLokaCoreString] start ====\n");
  loka::core::String hello = loka::core::String::Literal("Hello");
  loka::core::String space = loka::core::String::Literal(" ");
  loka::core::String world = loka::core::String::Literal("World");
  loka::core::String message = hello + space + world;
  class StubPlatformString : public loka::platform::String
  {
  public:
    explicit StubPlatformString(const std::string &value) : storage_(value) {}
    virtual bool appendUtf8(std::string &out) const
    {
      out.append(storage_);
      return true;
    }

  private:
    std::string storage_;
  };
  Managed<loka::platform::String> stubHandle = Managed<loka::platform::String>::Wrap(new StubPlatformString("!"));
  message = message + loka::core::String::FromPlatform(stubHandle);
  const std::string expected = "Hello World!";
  std::string flattened;
  assert(loka::platform::CollectUtf8(message, flattened));
  assert(flattened == expected);
  printf("[Loka::core::String] combined='%s'\n", flattened.c_str());
  loka::core::String concat = loka::core::String::Literal("Prefix ") + message + loka::core::String::Literal(" Suffix");
  std::string concatUtf8;
  assert(loka::platform::CollectUtf8(concat, concatUtf8));
  assert(concatUtf8 == "Prefix Hello World! Suffix");
  loka::core::String greetingFormat = loka::core::String::Format("%1, %2!", "Hello", loka::core::String::Literal("Declarative UI"));
  std::string greetingUtf8;
  assert(loka::platform::CollectUtf8(greetingFormat, greetingUtf8));
  assert(greetingUtf8 == "Hello, Declarative UI!");
  printf("==== [testLokaCoreString] end ====\n");
}

void testLokaCoreCollections()
{
  printf("\n==== [testLokaCoreCollections] start ====\n");
  loka::core::Array numbers;
  numbers.pushBack(loka::core::Value(10L));
  numbers.pushBack(loka::core::Value(20L));
  loka::core::Array copy = numbers;
  copy.pushBack(loka::core::Value(30L));
  assert(numbers.size() == 2);
  assert(copy.size() == 3);
  loka::core::Dictionary dict;
  dict.set(loka::core::String::Literal("title"), loka::core::Value(loka::core::String::Literal("inventory")));
  dict.set(loka::core::String::Literal("count"), loka::core::Value(static_cast<long>(numbers.size())));
  dict.set(loka::core::String::Literal("items"), loka::core::Value(numbers));
  loka::core::Value dictValue(dict);
  loka::core::Dictionary extracted = dictValue.asDictionary();
  const loka::core::Value *itemsValue = extracted.find(loka::core::String::Literal("items"));
  assert(itemsValue != 0);
  loka::core::Array extractedItems = itemsValue->asArray();
  assert(extractedItems.size() == numbers.size());
  const loka::core::Value *titleValue = extracted.find(loka::core::String::Literal("title"));
  assert(titleValue != 0);
  std::string titleUtf8;
  assert(loka::platform::CollectUtf8(titleValue->asString(), titleUtf8));
  assert(titleUtf8 == "inventory");
  bool removed = extracted.remove(loka::core::String::Literal("title"));
  assert(removed);
  assert(!extracted.hasKey(loka::core::String::Literal("title")));
  printf("[Loka::core::Dictionary] keys after remove=%zu\n", extracted.size());
  printf("==== [testLokaCoreCollections] end ====\n");
}
