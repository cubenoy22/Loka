#include "Tests.hpp"
#include <cassert>
#include "core/State.hpp"
#include "core/StateTracker.hpp"
#include "core/util/AutoTransactionGuard.hpp"
#include "core/util/StateUtil.hpp"
#include "core/SceneManager2.hpp"
#include "core2/scene/NodeComposition.hpp"
#include "core2/scene/NodeManager.hpp"
#include "core2/scene/PlatformController.hpp"
#include "app2/Box.hpp"
#include "app2/Button.hpp"

// --- main.cppから移動したテスト関数をここに実装 ---

void testDependencyPropagationCases()
{
  printf("\n==== [testDependencyPropagationCases] start ====\n");
  printf("testDependencyPropagationCases\n");
  // --- 多段依存 A→B→C ---
  MutableState<int> a;
  struct DerivedState : public StateBase
  {
    StateBase *dep;
    int value;
    DerivedState(StateBase *d) : dep(d), value(0) {}
    virtual bool recompute()
    {
      MutableState<int> *s = dynamic_cast<MutableState<int> *>(dep);
      DerivedState *d2 = dynamic_cast<DerivedState *>(dep);
      int old = value;
      if (s)
        value = s->get() + 1;
      else if (d2)
        value = d2->value + 1;
      printf("recompute: state=%p, old=%d, new=%d\n", (void *)this, old, value);
      return value != old;
    }
    virtual std::vector<StateBase *> getDependencyStates() const
    {
      return makeStateVector(dep, 0);
    }
  };
  DerivedState b(&a);
  DerivedState c(&b);
  std::vector<StateBase *> states = makeStateVector(&a, &b, &c, 0);
  PushStateTracker tracker(states);
  tracker.begin();
  a.set(10);
  tracker.end();
  assert(b.value == 11 && c.value == 12);
  tracker.begin();
  a.set(20);
  tracker.end();
  assert(b.value == 21 && c.value == 22);
  // --- 複数Derivedが1つのStateに依存 ---
  MutableState<int> s;
  DerivedState d1(&s);
  DerivedState d2(&s);
  std::vector<StateBase *> states2 = makeStateVector(&s, &d1, &d2, 0);
  PushStateTracker tracker2(states2);
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
  MutableState<int> s2;
  DerivedState d3(&s2);
  std::vector<StateBase *> states3 = makeStateVector(&s, &d1, &d2, &s2, &d3, 0);
  PushStateTracker tracker3(states3);
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
  MutableState<int> s_int(10);
  struct DoublePropEval : public DerivedState<int>::EvalFn
  {
    MutableState<int> *s;
    DoublePropEval(MutableState<int> *s_) : s(s_) {}
    int operator()() { return s->get() * 2; }
  };
  DerivedState<int> *doubleProp = new DerivedState<int>(std::vector<StateBase *>(1, &s_int), new DoublePropEval(&s_int));
  printf("[test] s_int=%p, doubleProp=%p\n", (void *)&s_int, (void *)doubleProp);
  std::vector<StateBase *> deps = doubleProp->getDependencyStates();
  for (size_t i = 0; i < deps.size(); ++i)
  {
    printf("[test] doubleProp.getDependencyStates()[%zu]=%p\n", i, (void *)deps[i]);
  }
  std::vector<StateBase *> trackerStates = makeStateVector(&s_int, doubleProp, 0);
  PushStateTracker tracker(trackerStates);
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

void testDeferredSideEffect()
{
  printf("\n==== [testDeferredSideEffect] start ====\n");
  MutableState<int> s_int(5);
  struct DoublePropEval : public DerivedState<int>::EvalFn
  {
    MutableState<int> *s;
    DoublePropEval(MutableState<int> *s_) : s(s_) {}
    int operator()() { return s->get() * 2; }
  };
  DerivedState<int> *doubleProp = new DerivedState<int>(std::vector<StateBase *>(1, &s_int), new DoublePropEval(&s_int));
  std::vector<StateBase *> trackerStatesDeferred = makeStateVector(&s_int, doubleProp, 0);
  PushStateTracker tracker(trackerStatesDeferred);
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
  MutableState<std::string> name("");
  struct IsValidEval : public DerivedState<bool>::EvalFn
  {
    MutableState<std::string> *n;
    IsValidEval(MutableState<std::string> *n_) : n(n_) {}
    bool operator()() { return n->get().length() >= 3; }
  };
  DerivedState<bool> *isValid = new DerivedState<bool>(std::vector<StateBase *>(1, &name), new IsValidEval(&name));
  std::vector<StateBase *> trackerStatesText = makeStateVector(&name, isValid, 0);
  PushStateTracker tracker(trackerStatesText);
  struct ValidCallback
  {
    static void onChange(void *userData)
    {
      DerivedState<bool> *isValid = (DerivedState<bool> *)userData;
      printf("[isValid] changed: %s\n", isValid->get() ? "OK" : "NG");
    }
  };
  printf("[TextInput] name.set(\"ab\")\n");
  tracker.begin();
  name.set("ab");
  tracker.end();
  assert(isValid->get() == false);
  printf("isValid: %s\n", isValid->get() ? "OK" : "NG");
  printf("[TextInput] name.set(\"senko\")\n");
  tracker.begin();
  name.set("senko");
  tracker.end();
  assert(isValid->get() == true);
  printf("isValid: %s\n", isValid->get() ? "OK" : "NG");
  printf("==== [testTextInputOnChange] end ====\n");
}

void testBatchTransaction()
{
  printf("\n==== [testBatchTransaction] start ====\n");
  MutableState<int> s1(1);
  struct SumPropEval : public DerivedState<int>::EvalFn
  {
    MutableState<int> *s;
    SumPropEval(MutableState<int> *s_) : s(s_) {}
    int operator()() { return s->get() * 2; }
  };
  DerivedState<int> *sumProp = new DerivedState<int>(std::vector<StateBase *>(1, &s1), new SumPropEval(&s1));
  MutableState<int> s2(2);
  std::vector<StateBase *> trackerStatesBatch = makeStateVector(&s1, &s2, sumProp, 0);
  PushStateTracker tracker(trackerStatesBatch);
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
  MutableState<int> s(0);
  struct DoublePropEval : public DerivedState<int>::EvalFn
  {
    MutableState<int> *s;
    DoublePropEval(MutableState<int> *s_) : s(s_) {}
    int operator()() { return s->get() * 2; }
  };
  DerivedState<int> *doubleProp = new DerivedState<int>(std::vector<StateBase *>(1, &s), new DoublePropEval(&s));
  std::vector<StateBase *> trackerStatesRAII = makeStateVector(&s, doubleProp, 0);
  PushStateTracker tracker(trackerStatesRAII);
  {
    AutoTransactionGuard _(&tracker);
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
    std::string name;
    std::string email;
    int age;
    bool agree;
  };
  MutableState<std::string> name("");
  MutableState<std::string> email("");
  MutableState<int> age(0);
  MutableState<bool> agree(false);
  std::vector<StateBase *> deps;
  deps.push_back(&name);
  deps.push_back(&email);
  deps.push_back(&age);
  deps.push_back(&agree);
  struct IsValidEval : public DerivedState<bool>::EvalFn
  {
    MutableState<std::string> *name;
    MutableState<std::string> *email;
    MutableState<int> *age;
    MutableState<bool> *agree;
    IsValidEval(MutableState<std::string> *n, MutableState<std::string> *e, MutableState<int> *a, MutableState<bool> *ag)
        : name(n), email(e), age(a), agree(ag) {}
    bool operator()()
    {
      return !name->get().empty() && !email->get().empty() && age->get() >= 18 && agree->get();
    }
  };
  DerivedState<bool> *isValid = new DerivedState<bool>(deps, new IsValidEval(&name, &email, &age, &agree));
  StateBase *deps2[5];
  for (size_t i = 0; i < deps.size(); ++i)
    deps2[i] = deps[i];
  deps2[deps.size()] = isValid;
  PushStateTracker tracker(std::vector<StateBase *>(deps2, deps2 + 5));
  struct Callback
  {
    static void onChange(bool v, void *)
    {
      printf("[isValid] changed: %s\n", v ? "true" : "false");
    }
  };
  isValid->bind((StateBase::OnChangeFn)Callback::onChange, isValid, false);
  tracker.begin();
  name.set("senko");
  email.set("");
  age.set(20);
  agree.set(true);
  tracker.end();
  assert(isValid->get() == false); // emailが空

  tracker.begin();
  email.set("senko@ai");
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
  using namespace declara::core::scene;
  using namespace declara::app;

  NodeComposition composition;
  BoxProps boxProps;
  BoxDefinition box(boxProps);
  BoxDefinition &root = composition.declare(box);

  ButtonProps buttonProps;
  buttonProps.setText("Hello");
  ButtonDefinition button(buttonProps);

  root << button;

  Node *tree = composition.createNodeTree();
  assert(tree != NULL);

  BoxNode *boxNode = dynamic_cast<BoxNode *>(tree);
  assert(boxNode != NULL);
  const std::vector<Node *> &children = boxNode->getChildren();
  assert(children.size() == 1);
  ButtonNode *buttonNode = dynamic_cast<ButtonNode *>(children[0]);
  assert(buttonNode != NULL);

  delete tree;
}

void testStaticNodeManagerRun()
{
  using namespace declara::core::scene;
  using namespace declara::app;

  class DummyScene : public Scene
  {
  public:
    virtual void compose(NodeComposition &composition)
    {
      ButtonProps buttonProps;
      buttonProps.setText("Controller");
      ButtonDefinition button(buttonProps);
      composition.declare(button);
    }
  };

  class DummyPlatformController : public IPlatformController
  {
  public:
    DummyPlatformController() : lastMaterialized_(0), destroyed_(false) {}
    virtual void materialize(Node *rootNode)
    {
      lastMaterialized_ = rootNode;
    }
    virtual void synchronize() {}
    virtual void destroy() { destroyed_ = true; }

    Node *lastMaterialized_;
    bool destroyed_;
  };

  DummyScene scene;
  DummyPlatformController platform;
  {
    StaticNodeManager manager;
    manager.mount(&scene, &platform);
    assert(platform.lastMaterialized_ != 0);
    assert(!platform.destroyed_);
  }
  assert(platform.destroyed_);
}
