#include "Tests.hpp"
#include <cassert>
#include <string>
#include "loka/core/State.hpp"
#include "loka/core/StateTracker.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"
#include "loka/core/util/StateUtil.hpp"
#include "app/SceneManager2.hpp"
#include "app/scene/NodeComposition.hpp"
#include "app/scene/PlatformController.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/node/StaticComposition.hpp"
#include "app/scene/node/DynamicComposition.hpp"
#include "app/Box.hpp"
#include "app/Button.hpp"
#include "app/Text.hpp"
#include "loka/core/Managed.hpp"
#include "loka/core/String.hpp"
#include "loka/core/Value.hpp"
#include "loka/core/Vector.hpp"
#include "loka/platform/StringUTF8.hpp"
#include "loka/platform/String.hpp"
#include "loka/dsl/dsl.hpp"
#include "loka/dsl/testing/SceneTestFlow.hpp"

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
      return loka::core::makeStateVector(dep, STATE_NULL);
    }
  };
  DerivedState b(&a);
  DerivedState c(&b);
  std::vector<loka::core::StateBase *> states = loka::core::makeStateVector(&a, &b, &c, STATE_NULL);
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
  std::vector<loka::core::StateBase *> states2 = loka::core::makeStateVector(&s, &d1, &d2, STATE_NULL);
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
  std::vector<loka::core::StateBase *> states3 = loka::core::makeStateVector(&s, &d1, &d2, &s2, &d3, STATE_NULL);
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
  loka::core::DerivedState<int> *doubleProp = new loka::core::DerivedState<int>(loka::core::makeStateVector(&s_int, STATE_NULL), new DoublePropEval(&s_int));
  printf("[test] s_int=%p, doubleProp=%p\n", (void *)&s_int, (void *)doubleProp);
  std::vector<loka::core::StateBase *> deps = doubleProp->getDependencyStates();
  for (size_t i = 0; i < deps.size(); ++i)
  {
    printf("[test] doubleProp.getDependencyStates()[%zu]=%p\n", i, (void *)deps[i]);
  }
  std::vector<loka::core::StateBase *> trackerStates = loka::core::makeStateVector(&s_int, doubleProp, STATE_NULL);
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
  struct DummyOwner : public loka::app::scene::IStateOwner
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
  loka::app::scene::BoundState<int> states[17];
  {
    loka::app::scene::NodeComposition::StateBatch batch(&owner);
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
  loka::core::DerivedState<int> *doubleProp = new loka::core::DerivedState<int>(loka::core::makeStateVector(&s_int, STATE_NULL), new DoublePropEval(&s_int));
  std::vector<loka::core::StateBase *> trackerStatesDeferred = loka::core::makeStateVector(&s_int, doubleProp, STATE_NULL);
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
  loka::core::DerivedState<bool> *isValid = new loka::core::DerivedState<bool>(loka::core::makeStateVector(&name, STATE_NULL), new IsValidEval(&name));
  std::vector<loka::core::StateBase *> trackerStatesText = loka::core::makeStateVector(&name, isValid, STATE_NULL);
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
  loka::core::DerivedState<int> *sumProp = new loka::core::DerivedState<int>(loka::core::makeStateVector(&s1, STATE_NULL), new SumPropEval(&s1));
  loka::core::MutableState<int> s2(2);
  std::vector<loka::core::StateBase *> trackerStatesBatch = loka::core::makeStateVector(&s1, &s2, sumProp, STATE_NULL);
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
  loka::core::DerivedState<int> *doubleProp = new loka::core::DerivedState<int>(loka::core::makeStateVector(&s, STATE_NULL), new DoublePropEval(&s));
  std::vector<loka::core::StateBase *> trackerStatesRAII = loka::core::makeStateVector(&s, doubleProp, STATE_NULL);
  loka::core::PushStateTracker tracker(trackerStatesRAII);
  {
    loka::core::StateTrackerGuard _(&tracker);
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
  std::vector<loka::core::StateBase *> deps = loka::core::makeStateVector(&name, &email, &age, &agree, STATE_NULL);
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
  std::vector<loka::core::StateBase *> trackerDeps = loka::core::makeStateVector(&name, &email, &age, &agree, isValid, STATE_NULL);
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
  using namespace loka::app::scene;
  using namespace loka::app;

  loka::app::scene::NodeComposition composition;
  BoxDefinition &root = composition.declare(Box().testId("RootBox"));
  ButtonDefinition button = Button("Hello").testId();

  root << button;

  Node *tree = composition.createNodeTree();
  assert(tree != NULL);

  BoxNode *boxNode = dynamic_cast<BoxNode *>(tree);
  assert(boxNode != NULL);
  assert(boxNode->testId() == "RootBox");
  Node *child = boxNode->childrenHead();
  size_t childCount = boxNode->childrenCount();
  assert(child != NULL);
  assert(childCount == 1);
  ButtonNode *buttonNode = dynamic_cast<ButtonNode *>(child);
  assert(buttonNode != NULL);
  assert(!buttonNode->testId().empty());
  assert(buttonNode->testId().find("auto-") == 0);

  delete tree;
}

void testNodeCompositionShowIf()
{
  using namespace loka::app;
  using namespace loka::app::scene;

  loka::core::MutableState<bool> show;
  show.set(true, true);

  NodeComposition composition;
  BoxDefinition &root = composition.declare(Box());
  ButtonDefinition trueButton = Button("Shown");
  TextDefinition falseText = Text("Hidden");
  root << composition.showIf(show, trueButton, falseText);

  Node *tree = composition.createNodeTree();
  assert(tree != 0);

  BoxNode *boxNode = dynamic_cast<BoxNode *>(tree);
  assert(boxNode != 0);
  Node *conditionalNodeBase = boxNode->childrenHead();
  assert(conditionalNodeBase != 0);

  ConditionalNode *conditionalNode = dynamic_cast<ConditionalNode *>(conditionalNodeBase);
  assert(conditionalNode != 0);
  conditionalNode->compose();
  assert(conditionalNode->activeNode != 0);
  assert(dynamic_cast<ButtonNode *>(conditionalNode->activeNode) != 0);

  {
    loka::core::PushStateTracker tracker(loka::core::makeStateVector(&show, STATE_NULL));
    loka::core::StateTrackerGuard guard(&tracker);
    show.set(false, true);
  }

  assert(conditionalNode->activeNode != 0);
  assert(dynamic_cast<TextNode *>(conditionalNode->activeNode) != 0);

  delete tree;
}

void testSceneMountLifecycle()
{
  using namespace loka::app::scene;

  class DummyPlatformController : public IPlatformController
  {
  public:
    DummyPlatformController() : lastMaterialized_(0), destroyed_(false) {}
    virtual void onChange(Node *rootNode, loka::app::scene::NodeDirtyFlags flags)
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
  int g_dynamicChildAttachComposeCount = 0;
  int g_dynamicChildUpdateEventCount = 0;
  loka::core::MutableState<bool> *g_boundaryObservedState = 0;

  class ChildBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<ChildBoundaryNode> ChildBoundaryProps;

  class ChildBoundaryNode : public loka::app::scene::BoundaryNodeFor<ChildBoundaryNode>
  {
  public:
    ChildBoundaryNode(const ChildBoundaryProps &p)
        : loka::app::scene::BoundaryNodeFor<ChildBoundaryNode>(ChildBoundaryProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      ++g_childComposeCount;
    }
  };

  loka::app::scene::BoundaryDefinition<ChildBoundaryProps, ChildBoundaryNode> ChildBoundary()
  {
    return loka::app::scene::Boundary<ChildBoundaryNode>();
  }

  class RootBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<RootBoundaryNode> RootBoundaryProps;

  class RootBoundaryNode : public loka::app::scene::BoundaryNodeFor<RootBoundaryNode>
  {
  public:
    RootBoundaryNode(const RootBoundaryProps &p)
        : loka::app::scene::BoundaryNodeFor<RootBoundaryNode>(RootBoundaryProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      ++g_rootComposeCount;
      c.declare(ChildBoundary());
    }
  };

  loka::app::scene::BoundaryDefinition<RootBoundaryProps, RootBoundaryNode> RootBoundary()
  {
    return loka::app::scene::Boundary<RootBoundaryNode>();
  }

  class ChildDynamicBoundaryNode;
  typedef loka::app::scene::DynamicCompositionPropsFor<ChildDynamicBoundaryNode> ChildDynamicBoundaryProps;

  class ChildDynamicBoundaryNode : public loka::app::scene::DynamicCompositionNodeFor<ChildDynamicBoundaryNode>
  {
  public:
    ChildDynamicBoundaryNode(const ChildDynamicBoundaryProps &p)
        : loka::app::scene::DynamicCompositionNodeFor<ChildDynamicBoundaryNode>(ChildDynamicBoundaryProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      ++g_dynamicChildAttachComposeCount;
    }

    virtual void composeWithContext(loka::app::scene::ComponentContext &context, loka::app::scene::ComposeEvent event)
    {
      if (event == loka::app::scene::COMPOSE_EVENT_UPDATE)
      {
        ++g_dynamicChildUpdateEventCount;
      }
      loka::app::scene::DynamicCompositionNodeFor<ChildDynamicBoundaryNode>::composeWithContext(context, event);
    }
  };

  loka::app::scene::BoundaryDefinition<ChildDynamicBoundaryProps, ChildDynamicBoundaryNode> ChildDynamicBoundary()
  {
    return loka::app::scene::DynamicCompositionBoundary<ChildDynamicBoundaryNode>();
  }

  class RootStaticWithDynamicChildNode;
  typedef loka::app::scene::BoundaryPropsFor<RootStaticWithDynamicChildNode> RootStaticWithDynamicChildProps;

  class RootStaticWithDynamicChildNode : public loka::app::scene::BoundaryNodeFor<RootStaticWithDynamicChildNode>
  {
  public:
    RootStaticWithDynamicChildNode(const RootStaticWithDynamicChildProps &p)
        : loka::app::scene::BoundaryNodeFor<RootStaticWithDynamicChildNode>(RootStaticWithDynamicChildProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(ChildDynamicBoundary());
    }
  };

  loka::app::scene::BoundaryDefinition<RootStaticWithDynamicChildProps, RootStaticWithDynamicChildNode> RootStaticWithDynamicChildBoundary()
  {
    return loka::app::scene::Boundary<RootStaticWithDynamicChildNode>();
  }

  int g_dynamicRootComposeCount = 0;
  int g_dynamicRootUpdateEventCount = 0;

  class RootDynamicBoundaryNode;
  typedef loka::app::scene::DynamicCompositionPropsFor<RootDynamicBoundaryNode> RootDynamicBoundaryProps;

  class RootDynamicBoundaryNode : public loka::app::scene::DynamicCompositionNodeFor<RootDynamicBoundaryNode>
  {
  public:
    RootDynamicBoundaryNode(const RootDynamicBoundaryProps &p)
        : loka::app::scene::DynamicCompositionNodeFor<RootDynamicBoundaryNode>(RootDynamicBoundaryProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      ++g_dynamicRootComposeCount;
    }

    virtual void composeWithContext(loka::app::scene::ComponentContext &context, loka::app::scene::ComposeEvent event)
    {
      if (event == loka::app::scene::COMPOSE_EVENT_UPDATE)
      {
        ++g_dynamicRootUpdateEventCount;
      }
      loka::app::scene::DynamicCompositionNodeFor<RootDynamicBoundaryNode>::composeWithContext(context, event);
    }
  };

  loka::app::scene::BoundaryDefinition<RootDynamicBoundaryProps, RootDynamicBoundaryNode> RootDynamicBoundary()
  {
    return loka::app::scene::DynamicCompositionBoundary<RootDynamicBoundaryNode>();
  }

  class StaticObservedBoundaryNode;
  typedef loka::app::scene::BoundaryPropsFor<StaticObservedBoundaryNode> StaticObservedBoundaryProps;

  class StaticObservedBoundaryNode : public loka::app::scene::BoundaryNodeFor<StaticObservedBoundaryNode>
  {
  public:
    StaticObservedBoundaryNode(const StaticObservedBoundaryProps &p)
        : loka::app::scene::BoundaryNodeFor<StaticObservedBoundaryNode>(StaticObservedBoundaryProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
    }

    virtual void declareObservedStates(loka::app::scene::ObservedStateRegistrar &registrar)
    {
      if (g_boundaryObservedState)
      {
        registrar.observe(g_boundaryObservedState, loka::app::scene::NODE_DIRTY_PROPS);
      }
    }
  };

  loka::app::scene::BoundaryDefinition<StaticObservedBoundaryProps, StaticObservedBoundaryNode> StaticObservedBoundary()
  {
    return loka::app::scene::Boundary<StaticObservedBoundaryNode>();
  }

  class DynamicObservedBoundaryNode;
  typedef loka::app::scene::DynamicCompositionPropsFor<DynamicObservedBoundaryNode> DynamicObservedBoundaryProps;

  class DynamicObservedBoundaryNode : public loka::app::scene::DynamicCompositionNodeFor<DynamicObservedBoundaryNode>
  {
  public:
    DynamicObservedBoundaryNode(const DynamicObservedBoundaryProps &p)
        : loka::app::scene::DynamicCompositionNodeFor<DynamicObservedBoundaryNode>(DynamicObservedBoundaryProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
    }

    virtual void declareObservedStates(loka::app::scene::ObservedStateRegistrar &registrar)
    {
      if (g_boundaryObservedState)
      {
        registrar.observe(g_boundaryObservedState, loka::app::scene::NODE_DIRTY_PROPS);
      }
    }
  };

  loka::app::scene::BoundaryDefinition<DynamicObservedBoundaryProps, DynamicObservedBoundaryNode> DynamicObservedBoundary()
  {
    return loka::app::scene::DynamicCompositionBoundary<DynamicObservedBoundaryNode>();
  }

  class StaticObservedHostNode;
  typedef loka::app::scene::BoundaryPropsFor<StaticObservedHostNode> StaticObservedHostProps;

  class StaticObservedHostNode : public loka::app::scene::BoundaryNodeFor<StaticObservedHostNode>
  {
  public:
    StaticObservedHostNode(const StaticObservedHostProps &p)
        : loka::app::scene::BoundaryNodeFor<StaticObservedHostNode>(StaticObservedHostProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(StaticObservedBoundary());
    }
  };

  loka::app::scene::BoundaryDefinition<StaticObservedHostProps, StaticObservedHostNode> StaticObservedHostBoundary()
  {
    return loka::app::scene::Boundary<StaticObservedHostNode>();
  }

  class DynamicObservedHostNode;
  typedef loka::app::scene::BoundaryPropsFor<DynamicObservedHostNode> DynamicObservedHostProps;

  class DynamicObservedHostNode : public loka::app::scene::BoundaryNodeFor<DynamicObservedHostNode>
  {
  public:
    DynamicObservedHostNode(const DynamicObservedHostProps &p)
        : loka::app::scene::BoundaryNodeFor<DynamicObservedHostNode>(DynamicObservedHostProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(DynamicObservedBoundary());
    }
  };

  loka::app::scene::BoundaryDefinition<DynamicObservedHostProps, DynamicObservedHostNode> DynamicObservedHostBoundary()
  {
    return loka::app::scene::Boundary<DynamicObservedHostNode>();
  }
}

void testSceneBoundaryNestedCompose()
{
  using loka::app::scene::IPlatformController;
  using loka::app::scene::Node;
  using loka::app::scene::Scene;

  class DummyPlatformController : public IPlatformController
  {
  public:
    DummyPlatformController() : lastMaterialized_(0), destroyed_(false) {}
    virtual void onChange(Node *rootNode, loka::app::scene::NodeDirtyFlags flags)
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

void testStaticBoundaryPropagatesUpdateToDynamicChild()
{
  using loka::app::scene::IPlatformController;
  using loka::app::scene::Node;
  using loka::app::scene::Scene;

  class DummyPlatformController : public IPlatformController
  {
  public:
    DummyPlatformController() : lastMaterialized_(0), destroyed_(false) {}
    virtual void onChange(Node *rootNode, loka::app::scene::NodeDirtyFlags flags)
    {
      (void)flags;
      lastMaterialized_ = rootNode;
    }
    virtual void synchronize() {}
    virtual void destroy() { destroyed_ = true; }

    Node *lastMaterialized_;
    bool destroyed_;
  };

  g_dynamicChildAttachComposeCount = 0;
  g_dynamicChildUpdateEventCount = 0;

  Scene scene(RootStaticWithDynamicChildBoundary());
  DummyPlatformController platform;
  scene.mount(&platform);
  scene.updateAttached(true);

  assert(g_dynamicChildAttachComposeCount == 1);
  assert(g_dynamicChildUpdateEventCount == 0);

  scene.invalidate();

  assert(g_dynamicChildAttachComposeCount == 1);
  assert(g_dynamicChildUpdateEventCount == 1);

  scene.unmount();
}

void testDynamicBoundaryRecomposesOnlyOnChildDirty()
{
  using loka::app::scene::IPlatformController;
  using loka::app::scene::Node;
  using loka::app::scene::NodeDirtyFlags;
  using loka::app::scene::Scene;

  class DummyPlatformController : public IPlatformController
  {
  public:
    DummyPlatformController() : lastMaterialized_(0), destroyed_(false), lastFlags_(loka::app::scene::NODE_DIRTY_NONE) {}
    virtual void onChange(Node *rootNode, NodeDirtyFlags flags)
    {
      lastMaterialized_ = rootNode;
      lastFlags_ = flags;
    }
    virtual void synchronize() {}
    virtual void destroy() { destroyed_ = true; }

    Node *lastMaterialized_;
    bool destroyed_;
    NodeDirtyFlags lastFlags_;
  };

  g_dynamicRootComposeCount = 0;
  g_dynamicRootUpdateEventCount = 0;

  Scene scene(RootDynamicBoundary());
  DummyPlatformController platform;
  scene.mount(&platform);
  scene.updateAttached(true);

  assert(g_dynamicRootComposeCount == 1);
  assert(g_dynamicRootUpdateEventCount == 0);

  scene.invalidate(loka::app::scene::NODE_DIRTY_PROPS);
  assert(g_dynamicRootComposeCount == 1);
  assert(g_dynamicRootUpdateEventCount == 1);

  scene.invalidate(loka::app::scene::NODE_DIRTY_LAYOUT);
  assert(g_dynamicRootComposeCount == 1);
  assert(g_dynamicRootUpdateEventCount == 2);

  scene.invalidate(loka::app::scene::NODE_DIRTY_CHILD);
  assert(g_dynamicRootComposeCount == 2);
  assert(g_dynamicRootUpdateEventCount == 3);

  scene.unmount();
}

void testBoundaryDirtyPolicyStaticImmediateDynamicDeferred()
{
  using loka::app::scene::IPlatformController;
  using loka::app::scene::Node;
  using loka::app::scene::NodeDirtyFlags;
  using loka::app::scene::Scene;

  class DirtyCapturePlatformController : public IPlatformController
  {
  public:
    DirtyCapturePlatformController() : calls_(0), lastFlags_(loka::app::scene::NODE_DIRTY_NONE) {}
    virtual void onChange(Node *rootNode, NodeDirtyFlags flags)
    {
      (void)rootNode;
      lastFlags_ = flags;
      ++calls_;
    }
    virtual void synchronize() {}
    virtual void destroy() {}

    int calls_;
    NodeDirtyFlags lastFlags_;
  };

  loka::core::MutableState<bool> observedState(false);
  g_boundaryObservedState = &observedState;

  {
    Scene scene(StaticObservedHostBoundary());
    DirtyCapturePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);
    assert(platform.calls_ == 1);

    loka::app::scene::BoundaryNode *boundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
    boundary = boundary ? boundary->childrenHead() ? boundary->childrenHead()->asBoundary() : 0 : 0;
    assert(boundary != 0);
    {
      loka::core::StateTrackerGuard guard(boundary->tracker());
      observedState.set(true);
    }

    assert(platform.calls_ == 2);
    assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_PROPS) != 0);
    scene.unmount();
  }

  observedState.set(false, true);

  {
    Scene scene(DynamicObservedHostBoundary());
    DirtyCapturePlatformController platform;
    scene.mount(&platform);
    scene.updateAttached(true);
    assert(platform.calls_ == 1);

    loka::app::scene::BoundaryNode *boundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
    boundary = boundary ? boundary->childrenHead() ? boundary->childrenHead()->asBoundary() : 0 : 0;
    assert(boundary != 0);
    {
      loka::core::StateTrackerGuard guard(boundary->tracker());
      observedState.set(true);
    }

    assert(platform.calls_ == 1);
    scene.flushInvalidation();
    assert(platform.calls_ == 2);
    assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_PROPS) != 0);
    scene.unmount();
  }

  g_boundaryObservedState = 0;
}

void testSceneInvalidateUsesRequestedDirtyFlags()
{
  using loka::app::scene::IPlatformController;
  using loka::app::scene::Node;
  using loka::app::scene::NodeDirtyFlags;
  using loka::app::scene::Scene;

  class DirtyCapturePlatformController : public IPlatformController
  {
  public:
    DirtyCapturePlatformController() : lastMaterialized_(0), destroyed_(false), lastFlags_(loka::app::scene::NODE_DIRTY_NONE), calls_(0) {}
    virtual void onChange(Node *rootNode, NodeDirtyFlags flags)
    {
      lastMaterialized_ = rootNode;
      lastFlags_ = flags;
      ++calls_;
    }
    virtual void synchronize() {}
    virtual void destroy() { destroyed_ = true; }

    Node *lastMaterialized_;
    bool destroyed_;
    NodeDirtyFlags lastFlags_;
    int calls_;
  };

  Scene scene(RootBoundary());
  DirtyCapturePlatformController platform;
  scene.mount(&platform);
  scene.updateAttached(true);
  assert(platform.lastMaterialized_ != 0);
  assert(platform.calls_ == 1);
  assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_INITIAL) != 0);

  scene.invalidate(loka::app::scene::NODE_DIRTY_LAYOUT);
  assert(platform.calls_ == 2);
  assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_LAYOUT) != 0);

  scene.invalidate(static_cast<loka::app::scene::NodeDirtyFlags>(loka::app::scene::NODE_DIRTY_LAYOUT | loka::app::scene::NODE_DIRTY_CHILD));
  assert(platform.calls_ == 3);
  assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_LAYOUT) != 0);
  assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_CHILD) != 0);

  scene.unmount();
}

void testSceneRequestInvalidateDefersUntilFlush()
{
  using loka::app::scene::IPlatformController;
  using loka::app::scene::Node;
  using loka::app::scene::NodeDirtyFlags;
  using loka::app::scene::Scene;

  class DirtyCapturePlatformController : public IPlatformController
  {
  public:
    DirtyCapturePlatformController() : lastMaterialized_(0), destroyed_(false), lastFlags_(loka::app::scene::NODE_DIRTY_NONE), calls_(0) {}
    virtual void onChange(Node *rootNode, NodeDirtyFlags flags)
    {
      lastMaterialized_ = rootNode;
      lastFlags_ = flags;
      ++calls_;
    }
    virtual void synchronize() {}
    virtual void destroy() { destroyed_ = true; }

    Node *lastMaterialized_;
    bool destroyed_;
    NodeDirtyFlags lastFlags_;
    int calls_;
  };

  Scene scene(RootBoundary());
  DirtyCapturePlatformController platform;
  scene.mount(&platform);
  scene.updateAttached(true);
  assert(platform.calls_ == 1);

  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_LAYOUT);
  assert(platform.calls_ == 1); // still deferred

  const bool flushed = scene.flushInvalidation();
  assert(flushed);
  assert(platform.calls_ == 2);
  assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_LAYOUT) != 0);

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
  loka::core::Managed<loka::platform::String> stubHandle = loka::core::Managed<loka::platform::String>::Wrap(new StubPlatformString("!"));
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

void testNestedTransaction()
{
  printf("\n==== [testNestedTransaction] start ====\n");

  // --- Test 1: Nested StateTrackerGuard preserves both dirty changes ---
  {
    printf("[test1] Nested StateTrackerGuard\n");
    loka::core::MutableState<int> a(0);
    loka::core::MutableState<int> b(0);
    std::vector<loka::core::StateBase *> states = loka::core::makeStateVector(&a, &b, STATE_NULL);
    loka::core::PushStateTracker tracker(states);
    {
      loka::core::StateTrackerGuard outer(&tracker);
      a.set(10);
      {
        loka::core::StateTrackerGuard inner(&tracker);
        b.set(20);
      }
      // inner guard ended, but outer transaction still active
      // dirty info must be preserved
    }
    // outer guard ended, commit should have fired
    assert(a.get() == 10);
    assert(b.get() == 20);
    printf("[test1] a=%d (expect 10), b=%d (expect 20)\n", a.get(), b.get());
  }

  // --- Test 2: Nested direct begin/end works the same ---
  {
    printf("[test2] Nested direct begin/end\n");
    loka::core::MutableState<int> a(0);
    loka::core::MutableState<int> b(0);
    std::vector<loka::core::StateBase *> states = loka::core::makeStateVector(&a, &b, STATE_NULL);
    loka::core::PushStateTracker tracker(states);
    tracker.begin();
    a.set(100);
    tracker.begin(); // nested
    b.set(200);
    tracker.end(); // inner end - should be no-op
    // phase should still be PRECOMMIT (not IDLE)
    assert(tracker.phase() == loka::core::TRACKER_PRECOMMIT);
    tracker.end(); // outer end - should commit
    assert(a.get() == 100);
    assert(b.get() == 200);
    printf("[test2] a=%d (expect 100), b=%d (expect 200)\n", a.get(), b.get());
  }

  // --- Test 3: Deferred callbacks fire only at outermost end ---
  {
    printf("[test3] Deferred fires only at outermost end\n");
    loka::core::MutableState<int> s(0);
    std::vector<loka::core::StateBase *> states = loka::core::makeStateVector(&s, STATE_NULL);
    loka::core::PushStateTracker tracker(states);
    int deferredCount = 0;
    struct DeferredCallback
    {
      static void callback(void *userData) { ++(*static_cast<int *>(userData)); }
    };

    tracker.begin();
    s.set(1);
    tracker.defer(DeferredCallback::callback, &deferredCount);

    tracker.begin(); // nested
    s.set(2);
    tracker.defer(DeferredCallback::callback, &deferredCount);
    tracker.end(); // inner end - deferred must NOT fire yet
    assert(deferredCount == 0);

    tracker.end(); // outer end - both deferred callbacks fire
    assert(deferredCount == 2);
    assert(s.get() == 2);
    printf("[test3] deferred count=%d (expect 2), s=%d (expect 2)\n", deferredCount, s.get());
  }

  printf("==== [testNestedTransaction] end ====\n");
}

void testNestedTransactionInvalidateTiming()
{
  printf("\n==== [testNestedTransactionInvalidateTiming] start ====\n");

  // --- Test 1: nested guard must not invalidate before outer guard ends ---
  {
    printf("[test1] nested guard invalidate timing\n");
    loka::core::MutableState<int> s(0);
    std::vector<loka::core::StateBase *> states = loka::core::makeStateVector(&s, STATE_NULL);
    loka::core::PushStateTracker tracker(states);
    int invalidateCount = 0;
    struct InvalidateCounter
    {
      static void callback(void *userData) { ++(*static_cast<int *>(userData)); }
    };

    {
      loka::core::StateTrackerGuard outer(&tracker, InvalidateCounter::callback, &invalidateCount);
      s.set(1);
      {
        loka::core::StateTrackerGuard inner(&tracker, InvalidateCounter::callback, &invalidateCount);
        s.set(2);
      }
      // Inner scope finished, but outer transaction still open.
      // Invalidate callback must not fire before outer guard exits.
      assert(invalidateCount == 0);
    }
    assert(invalidateCount == 1);
    assert(s.get() == 2);
    printf("[test1] invalidateCount=%d (expect 1), s=%d (expect 2)\n", invalidateCount, s.get());
  }

  // --- Test 2: consumeDirty inside nested transaction should not clear outer dirty ---
  {
    printf("[test2] nested consumeDirty visibility\n");
    loka::core::MutableState<int> s(0);
    std::vector<loka::core::StateBase *> states = loka::core::makeStateVector(&s, STATE_NULL);
    loka::core::PushStateTracker tracker(states);
    tracker.begin();
    s.set(10);
    tracker.begin(); // nested begin
    s.set(20);
    tracker.end(); // inner end
    bool consumedInInner = tracker.consumeDirty();
    assert(consumedInInner == false);
    tracker.end();
    bool consumedAfterOuter = tracker.consumeDirty();
    assert(consumedAfterOuter == true);
    assert(s.get() == 20);
    printf("[test2] inner=%s (expect false), outer=%s (expect true), s=%d (expect 20)\n",
           consumedInInner ? "true" : "false",
           consumedAfterOuter ? "true" : "false",
           s.get());
  }

  printf("==== [testNestedTransactionInvalidateTiming] end ====\n");
}
