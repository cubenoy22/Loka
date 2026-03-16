#include "Tests.hpp"
#include <cassert>
#include <string>
#include "loka/core/State.hpp"
#include "loka/core/StateTracker.hpp"
#include "loka/core/util/StateTrackerGuard.hpp"
#include "loka/core/util/StateUtil.hpp"
#include "app/SceneManager.hpp"
#include "app/scene/NodeComposition.hpp"
#include "app/scene/NodeCompositionCompare.hpp"
#include "app/scene/NodeCompositionDiff.hpp"
#include "app/scene/NodeCompositionTransaction.hpp"
#include "app/scene/PlatformController.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/node/StaticComposition.hpp"
#include "app/scene/node/DynamicComposition.hpp"
#include "app/Box.hpp"
#include "app/Button.hpp"
#include "app/Cell.hpp"
#include "app/OpenFileDialog.hpp"
#include "app/Window.hpp"
#include "app/PopupMenu.hpp"
#include "app/RowColumn.hpp"
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

void testNodeDefinitionTagPropagatesToCreatedNodes()
{
  using namespace loka::app;
  using namespace loka::app::scene;

  NodeComposition composition;
  BoxDefinition &root = static_cast<BoxDefinition &>(composition.declareTagged(100, Box()));
  ButtonDefinition button = Button("Hello");
  {
    NodeComposition::ChildComposition child(composition, root);
    child.composition().declareTagged(200, button);
  }

  Node *tree = composition.createNodeTree();
  assert(tree != 0);
  assert(tree->nodeTag() == 100);

  BoxNode *boxNode = static_cast<BoxNode *>(tree->asBoxNode());
  assert(boxNode != 0);
  Node *child = boxNode->childrenHead();
  assert(child != 0);
  assert(child->nodeTag() == 200);
  assert(child->asButtonNode() != 0);

  delete tree;
}

void testNodeCompositionDiffTracksEntries()
{
  using namespace loka::app::scene;

  NodeCompositionDiff diff;
  assert(diff.valid == false);
  assert(diff.fullRebuild == true);
  assert(diff.empty());
  assert(diff.entryCount() == 0);

  diff.valid = true;
  diff.fullRebuild = false;
  diff.addEntry(100, 0, NodeCompositionDiff::ACTION_RETAIN, 0, 0);
  diff.addEntry(200, 1, NodeCompositionDiff::ACTION_REPLACE, 1, 1);
  diff.addEntry(300, 2, NodeCompositionDiff::ACTION_RETIRE, 2, -1);

  assert(!diff.empty());
  assert(diff.entryCount() == 3);

  NodeCompositionDiff::Entry *entry = diff.entriesHead();
  assert(entry != 0);
  assert(entry->tag == 100);
  assert(entry->slot == 0);
  assert(entry->action == NodeCompositionDiff::ACTION_RETAIN);
  entry = entry->nextInComposition;
  assert(entry != 0);
  assert(entry->tag == 200);
  assert(entry->action == NodeCompositionDiff::ACTION_REPLACE);
  entry = entry->nextInComposition;
  assert(entry != 0);
  assert(entry->tag == 300);
  assert(entry->action == NodeCompositionDiff::ACTION_RETIRE);
  assert(entry->currentIndex == -1);

  diff.clear();
  assert(diff.valid == false);
  assert(diff.fullRebuild == true);
  assert(diff.empty());
  assert(diff.entryCount() == 0);
}

void testNodeCompositionTransactionTracksWorkingSet()
{
  using namespace loka::app::scene;

  NodeComposition previous;
  NodeComposition current;
  previous.declare(loka::app::VStack()
                   << loka::app::Text("Previous A").tag(10)
                   << loka::app::Text("Shared").tag(20));
  current.declare(loka::app::VStack()
                  << loka::app::Text("Shared updated").tag(20)
                  << loka::app::Text("Current C").tag(30));

  NodeCompositionTransaction transaction;
  assert(transaction.empty());
  assert(transaction.previous() == 0);
  assert(transaction.current() == 0);
  assert(transaction.retiredCount() == 0);
  assert(transaction.diff().empty());

  transaction.begin(&previous, &current);
  assert(!transaction.empty());
  assert(transaction.previous() == &previous);
  assert(transaction.current() == &current);
  assert(transaction.retiredCount() == 0);
  assert(transaction.diff().empty());

  bool built = transaction.buildDiffByTag();
  assert(built);
  assert(transaction.diff().valid);
  assert(!transaction.diff().fullRebuild);
  assert(transaction.diff().entryCount() == 2);
  transaction.noteRetiredChild();
  assert(transaction.retiredCount() == 1);

  transaction.clear();
  assert(transaction.empty());
  assert(transaction.previous() == 0);
  assert(transaction.current() == 0);
  assert(transaction.retiredCount() == 0);
  assert(transaction.diff().empty());
}

void testBuildNodeCompositionDiffByTagTracksRetainReplaceRetire()
{
  using namespace loka::app;
  using namespace loka::app::scene;

  NodeComposition previous;
  previous.declare(VStack()
                   << Text("Previous A").tag(100)
                   << Text("Shared").tag(200));

  NodeComposition current;
  current.declare(VStack()
                  << Text("Shared updated").tag(200)
                  << Text("Current C").tag(300));

  NodeCompositionDiff diff;
  bool built = buildNodeCompositionDiffByTag(previous, current, diff);
  assert(built);
  assert(diff.valid);
  assert(!diff.fullRebuild);
  assert(diff.entryCount() == 3);

  NodeCompositionDiff::Entry *entry = diff.entriesHead();
  assert(entry != 0);
  assert(entry->tag == 200);
  assert(entry->action == NodeCompositionDiff::ACTION_RETAIN);
  assert(entry->previousIndex == 1);
  assert(entry->currentIndex == 0);

  entry = entry->nextInComposition;
  assert(entry != 0);
  assert(entry->tag == 300);
  assert(entry->action == NodeCompositionDiff::ACTION_REPLACE);
  assert(entry->previousIndex == -1);
  assert(entry->currentIndex == 1);

  entry = entry->nextInComposition;
  assert(entry != 0);
  assert(entry->tag == 100);
  assert(entry->action == NodeCompositionDiff::ACTION_RETIRE);
  assert(entry->previousIndex == 0);
  assert(entry->currentIndex == -1);
}

void testSceneMountLifecycle()
{
  using namespace loka::app::scene;

  class DummyPlatformController : public IPlatformController
  {
  public:
    DummyPlatformController() : lastMaterialized_(0), destroyed_(false) {}
    virtual void onChange(Node *rootNode, loka::app::scene::NodeDirtyFlags flags, bool fullRebuild)
    {
      (void)flags;
      lastMaterialized_ = rootNode;
    }
    virtual void synchronize() {}
    virtual bool hasPendingSync() const { return false; }
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
  loka::core::MutableState<bool> *g_foreignBoundaryObservedState = 0;
  int g_foreignDynamicObservedComposeCount = 0;

  static loka::app::scene::Node *findNodeByTestIdRecursive(loka::app::scene::Node *node, const char *testId)
  {
    if (!node || !testId)
    {
      return 0;
    }
    if (node->testId() == testId)
    {
      return node;
    }
    loka::app::scene::INestable *nestable = node->asNestable();
    if (!nestable)
    {
      return 0;
    }
    loka::app::scene::Node *child = nestable->childrenHead();
    while (child)
    {
      loka::app::scene::Node *found = findNodeByTestIdRecursive(child, testId);
      if (found)
      {
        return found;
      }
      child = child->nextInComposition;
    }
    return 0;
  }

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
  int g_dynamicIdentityChildComposeCount = 0;
  int g_dynamicDetachChildAttachCount = 0;
  int g_dynamicDetachChildDetachCount = 0;
  bool g_dynamicDetachShowChild = true;
  int g_dynamicCallbackFireCount = 0;
  loka::core::EmitterState *g_dynamicCallbackEmitter = 0;
  loka::core::MutableState<bool> *g_dynamicIdentityPropsState = 0;
  loka::core::MutableState<int> *g_dynamicIdentityLayoutState = 0;
  loka::core::MutableState<bool> *g_dynamicIdentityChildState = 0;
  loka::core::MutableState<int> *g_popupSelectedIndexState = 0;
  loka::core::MutableState<bool> *g_popupEnabledState = 0;
  loka::core::MutableState<loka::app::FileChooserResult> *g_openFileResultState = 0;
  loka::core::MutableState<bool> *g_openFileVisibleState = 0;

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

  class DynamicIdentityChildNode;
  typedef loka::app::scene::BoundaryPropsFor<DynamicIdentityChildNode> DynamicIdentityChildProps;

  class DynamicIdentityChildNode : public loka::app::scene::BoundaryNodeFor<DynamicIdentityChildNode>
  {
  public:
    DynamicIdentityChildNode(const DynamicIdentityChildProps &p)
        : loka::app::scene::BoundaryNodeFor<DynamicIdentityChildNode>(DynamicIdentityChildProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      ++g_dynamicIdentityChildComposeCount;
    }
  };

  loka::app::scene::BoundaryDefinition<DynamicIdentityChildProps, DynamicIdentityChildNode> DynamicIdentityChildBoundary()
  {
    return loka::app::scene::Boundary<DynamicIdentityChildNode>();
  }

  class DynamicIdentityRootNode;
  typedef loka::app::scene::DynamicCompositionPropsFor<DynamicIdentityRootNode> DynamicIdentityRootProps;

  class DynamicIdentityRootNode : public loka::app::scene::DynamicCompositionNodeFor<DynamicIdentityRootNode>
  {
  public:
    DynamicIdentityRootNode(const DynamicIdentityRootProps &p)
        : loka::app::scene::DynamicCompositionNodeFor<DynamicIdentityRootNode>(DynamicIdentityRootProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(DynamicIdentityChildBoundary());
    }

    virtual void declareObservedStates(loka::app::scene::ObservedStateRegistrar &registrar)
    {
      if (g_dynamicIdentityPropsState)
      {
        registrar.observe(g_dynamicIdentityPropsState, loka::app::scene::NODE_DIRTY_PROPS);
      }
      if (g_dynamicIdentityLayoutState)
      {
        registrar.observe(g_dynamicIdentityLayoutState, loka::app::scene::NODE_DIRTY_LAYOUT);
      }
      if (g_dynamicIdentityChildState)
      {
        registrar.observe(g_dynamicIdentityChildState, loka::app::scene::NODE_DIRTY_CHILD);
      }
    }
  };

  loka::app::scene::BoundaryDefinition<DynamicIdentityRootProps, DynamicIdentityRootNode> DynamicIdentityRootBoundary()
  {
    return loka::app::scene::DynamicCompositionBoundary<DynamicIdentityRootNode>();
  }

  class DynamicIdentityHostNode;
  typedef loka::app::scene::BoundaryPropsFor<DynamicIdentityHostNode> DynamicIdentityHostProps;

  class DynamicIdentityHostNode : public loka::app::scene::BoundaryNodeFor<DynamicIdentityHostNode>
  {
  public:
    DynamicIdentityHostNode(const DynamicIdentityHostProps &p)
        : loka::app::scene::BoundaryNodeFor<DynamicIdentityHostNode>(DynamicIdentityHostProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(DynamicIdentityRootBoundary());
    }
  };

  loka::app::scene::BoundaryDefinition<DynamicIdentityHostProps, DynamicIdentityHostNode> DynamicIdentityHostBoundary()
  {
    return loka::app::scene::Boundary<DynamicIdentityHostNode>();
  }

  class DynamicDetachChildNode;
  typedef loka::app::scene::BoundaryPropsFor<DynamicDetachChildNode> DynamicDetachChildProps;

  class DynamicDetachChildNode : public loka::app::scene::BoundaryNodeFor<DynamicDetachChildNode>
  {
  public:
    DynamicDetachChildNode(const DynamicDetachChildProps &p)
        : loka::app::scene::BoundaryNodeFor<DynamicDetachChildNode>(DynamicDetachChildProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      ++g_dynamicDetachChildAttachCount;
    }

    virtual void composeWithContext(loka::app::scene::ComponentContext &context, loka::app::scene::ComposeEvent event)
    {
      if (event == loka::app::scene::COMPOSE_EVENT_DETACH)
      {
        ++g_dynamicDetachChildDetachCount;
      }
      loka::app::scene::BoundaryNodeFor<DynamicDetachChildNode>::composeWithContext(context, event);
    }
  };

  loka::app::scene::BoundaryDefinition<DynamicDetachChildProps, DynamicDetachChildNode> DynamicDetachChildBoundary()
  {
    return loka::app::scene::Boundary<DynamicDetachChildNode>();
  }

  class DynamicDetachRootNode;
  typedef loka::app::scene::DynamicCompositionPropsFor<DynamicDetachRootNode> DynamicDetachRootProps;

  class DynamicDetachRootNode : public loka::app::scene::DynamicCompositionNodeFor<DynamicDetachRootNode>
  {
  public:
    DynamicDetachRootNode(const DynamicDetachRootProps &p)
        : loka::app::scene::DynamicCompositionNodeFor<DynamicDetachRootNode>(DynamicDetachRootProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      if (g_dynamicDetachShowChild)
      {
        c.declare(DynamicDetachChildBoundary());
      }
    }
  };

  loka::app::scene::BoundaryDefinition<DynamicDetachRootProps, DynamicDetachRootNode> DynamicDetachRootBoundary()
  {
    return loka::app::scene::DynamicCompositionBoundary<DynamicDetachRootNode>();
  }

  class DynamicCallbackRootNode;
  typedef loka::app::scene::DynamicCompositionPropsFor<DynamicCallbackRootNode> DynamicCallbackRootProps;

  class DynamicCallbackRootNode : public loka::app::scene::DynamicCompositionNodeFor<DynamicCallbackRootNode>
  {
  public:
    DynamicCallbackRootNode(const DynamicCallbackRootProps &p)
        : loka::app::scene::DynamicCompositionNodeFor<DynamicCallbackRootNode>(DynamicCallbackRootProps(p)) {}

    void onEmitter()
    {
      ++g_dynamicCallbackFireCount;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      if (g_dynamicCallbackEmitter)
      {
        this->bindForUi(*g_dynamicCallbackEmitter, this, &DynamicCallbackRootNode::onEmitter);
      }
    }
  };

  loka::app::scene::BoundaryDefinition<DynamicCallbackRootProps, DynamicCallbackRootNode> DynamicCallbackRootBoundary()
  {
    return loka::app::scene::DynamicCompositionBoundary<DynamicCallbackRootNode>();
  }

  class PopupObservedHostNode;
  typedef loka::app::scene::BoundaryPropsFor<PopupObservedHostNode> PopupObservedHostProps;

  class PopupObservedHostNode : public loka::app::scene::BoundaryNodeFor<PopupObservedHostNode>
  {
  public:
    PopupObservedHostNode(const PopupObservedHostProps &p)
        : loka::app::scene::BoundaryNodeFor<PopupObservedHostNode>(PopupObservedHostProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      static const char *kItems[] = {"One", "Two", "Three"};
      loka::app::PopupMenu popup(kItems, 3);
      if (g_popupSelectedIndexState)
      {
        popup.selectedIndex(g_popupSelectedIndexState);
      }
      if (g_popupEnabledState)
      {
        popup.enabled(g_popupEnabledState);
      }
      c.declare(popup);
    }
  };

  loka::app::scene::BoundaryDefinition<PopupObservedHostProps, PopupObservedHostNode> PopupObservedHostBoundary()
  {
    return loka::app::scene::Boundary<PopupObservedHostNode>();
  }

  class OpenFileObservedHostNode;
  typedef loka::app::scene::BoundaryPropsFor<OpenFileObservedHostNode> OpenFileObservedHostProps;

  class OpenFileObservedHostNode : public loka::app::scene::BoundaryNodeFor<OpenFileObservedHostNode>
  {
  public:
    OpenFileObservedHostNode(const OpenFileObservedHostProps &p)
        : loka::app::scene::BoundaryNodeFor<OpenFileObservedHostNode>(OpenFileObservedHostProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      loka::app::OpenFileDialog dialog;
      if (g_openFileVisibleState)
      {
        dialog.isVisible(g_openFileVisibleState);
      }
      if (g_openFileResultState)
      {
        dialog.result(g_openFileResultState);
      }
      c.declare(dialog);
    }
  };

  loka::app::scene::BoundaryDefinition<OpenFileObservedHostProps, OpenFileObservedHostNode> OpenFileObservedHostBoundary()
  {
    return loka::app::scene::Boundary<OpenFileObservedHostNode>();
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

  class ForeignDynamicObservedBoundaryNode;
  typedef loka::app::scene::DynamicCompositionPropsFor<ForeignDynamicObservedBoundaryNode> ForeignDynamicObservedBoundaryProps;

  class ForeignDynamicObservedBoundaryNode : public loka::app::scene::DynamicCompositionNodeFor<ForeignDynamicObservedBoundaryNode>
  {
  public:
    ForeignDynamicObservedBoundaryNode(const ForeignDynamicObservedBoundaryProps &p)
        : loka::app::scene::DynamicCompositionNodeFor<ForeignDynamicObservedBoundaryNode>(ForeignDynamicObservedBoundaryProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      ++g_foreignDynamicObservedComposeCount;
    }

    virtual void declareObservedStates(loka::app::scene::ObservedStateRegistrar &registrar)
    {
      if (g_foreignBoundaryObservedState)
      {
        registrar.observe(g_foreignBoundaryObservedState, loka::app::scene::NODE_DIRTY_CHILD);
      }
    }
  };

  loka::app::scene::BoundaryDefinition<ForeignDynamicObservedBoundaryProps, ForeignDynamicObservedBoundaryNode> ForeignDynamicObservedBoundary()
  {
    return loka::app::scene::DynamicCompositionBoundary<ForeignDynamicObservedBoundaryNode>();
  }

  class ForeignDynamicObservedHostNode;
  typedef loka::app::scene::BoundaryPropsFor<ForeignDynamicObservedHostNode> ForeignDynamicObservedHostProps;

  class ForeignDynamicObservedHostNode : public loka::app::scene::BoundaryNodeFor<ForeignDynamicObservedHostNode>
  {
  public:
    ForeignDynamicObservedHostNode(const ForeignDynamicObservedHostProps &p)
        : loka::app::scene::BoundaryNodeFor<ForeignDynamicObservedHostNode>(ForeignDynamicObservedHostProps(p)), observed_() {}

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      c.declareStates().state(this->observed_, false);
      g_foreignBoundaryObservedState = this->observed_.mutableState();
    }

    virtual void detachNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      g_foreignBoundaryObservedState = 0;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(ForeignDynamicObservedBoundary());
    }

  private:
    loka::app::scene::BoundState<bool> observed_;
  };

  loka::app::scene::BoundaryDefinition<ForeignDynamicObservedHostProps, ForeignDynamicObservedHostNode> ForeignDynamicObservedHostBoundary()
  {
    return loka::app::scene::Boundary<ForeignDynamicObservedHostNode>();
  }

  class SampleLikeDynamicPaneNode;
  typedef loka::app::scene::DynamicCompositionPropsFor<SampleLikeDynamicPaneNode> SampleLikeDynamicPaneProps;

  class SampleLikeDynamicPaneNode : public loka::app::scene::DynamicCompositionNodeFor<SampleLikeDynamicPaneNode>
  {
  public:
    SampleLikeDynamicPaneNode(const SampleLikeDynamicPaneProps &p)
        : loka::app::scene::DynamicCompositionNodeFor<SampleLikeDynamicPaneNode>(SampleLikeDynamicPaneProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      ++g_foreignDynamicObservedComposeCount;
      if (g_foreignBoundaryObservedState && g_foreignBoundaryObservedState->get())
      {
        c.declare(loka::app::Text("Dynamic rebuild branch").testId("DynamicDetailsText"));
      }
      else
      {
        c.declare(loka::app::Button("Dynamic hidden branch").testId("DynamicDetailsButton"));
      }
    }

    virtual void declareObservedStates(loka::app::scene::ObservedStateRegistrar &registrar)
    {
      if (g_foreignBoundaryObservedState)
      {
        registrar.observe(g_foreignBoundaryObservedState, loka::app::scene::NODE_DIRTY_CHILD);
      }
    }
  };

  loka::app::scene::BoundaryDefinition<SampleLikeDynamicPaneProps, SampleLikeDynamicPaneNode> SampleLikeDynamicPaneBoundary()
  {
    return loka::app::scene::DynamicCompositionBoundary<SampleLikeDynamicPaneNode>();
  }

  class SampleLikeHostNode;
  typedef loka::app::scene::BoundaryPropsFor<SampleLikeHostNode> SampleLikeHostProps;

  class SampleLikeHostNode : public loka::app::scene::BoundaryNodeFor<SampleLikeHostNode>
  {
  public:
    SampleLikeHostNode(const SampleLikeHostProps &p)
        : loka::app::scene::BoundaryNodeFor<SampleLikeHostNode>(SampleLikeHostProps(p)), observed_() {}

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      c.declareStates().state(this->observed_, false);
      g_foreignBoundaryObservedState = this->observed_.mutableState();
    }

    virtual void detachNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      g_foreignBoundaryObservedState = 0;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(loka::app::HStack()
                << loka::app::Text("Static pane marker").testId("StaticMarker")
                << SampleLikeDynamicPaneBoundary());
    }

  private:
    loka::app::scene::BoundState<bool> observed_;
  };

  loka::app::scene::BoundaryDefinition<SampleLikeHostProps, SampleLikeHostNode> SampleLikeHostBoundary()
  {
    return loka::app::scene::Boundary<SampleLikeHostNode>();
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
    virtual void onChange(Node *rootNode, loka::app::scene::NodeDirtyFlags flags, bool fullRebuild)
    {
      (void)flags;
      lastMaterialized_ = rootNode;
    }
    virtual void synchronize() {}
    virtual bool hasPendingSync() const { return false; }
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
    virtual void onChange(Node *rootNode, loka::app::scene::NodeDirtyFlags flags, bool fullRebuild)
    {
      (void)flags;
      lastMaterialized_ = rootNode;
    }
    virtual void synchronize() {}
    virtual bool hasPendingSync() const { return false; }
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
    virtual void onChange(Node *rootNode, NodeDirtyFlags flags, bool fullRebuild)
    {
      lastMaterialized_ = rootNode;
      lastFlags_ = flags;
    }
    virtual void synchronize() {}
    virtual bool hasPendingSync() const { return false; }
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

void testDynamicBoundaryPreservesChildIdentityUntilChildDirty()
{
  using loka::app::scene::IPlatformController;
  using loka::app::scene::Node;
  using loka::app::scene::NodeDirtyFlags;
  using loka::app::scene::Scene;

  class DummyPlatformController : public IPlatformController
  {
  public:
    DummyPlatformController() : lastMaterialized_(0), lastFlags_(loka::app::scene::NODE_DIRTY_NONE), lastFullRebuild_(false) {}
    virtual void onChange(Node *rootNode, NodeDirtyFlags flags, bool fullRebuild)
    {
      lastMaterialized_ = rootNode;
      lastFlags_ = flags;
      lastFullRebuild_ = fullRebuild;
    }
    virtual void synchronize() {}
    virtual bool hasPendingSync() const { return false; }
    virtual void destroy() {}

    Node *lastMaterialized_;
    NodeDirtyFlags lastFlags_;
    bool lastFullRebuild_;
  };

  loka::core::MutableState<bool> propsState(false);
  loka::core::MutableState<int> layoutState(12);
  loka::core::MutableState<bool> childState(false);
  g_dynamicIdentityPropsState = &propsState;
  g_dynamicIdentityLayoutState = &layoutState;
  g_dynamicIdentityChildState = &childState;
  g_dynamicIdentityChildComposeCount = 0;

  Scene scene(DynamicIdentityHostBoundary());
  DummyPlatformController platform;
  scene.mount(&platform);
  scene.updateAttached(true);

  loka::app::scene::BoundaryNode *rootBoundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  rootBoundary = rootBoundary ? rootBoundary->childrenHead() ? rootBoundary->childrenHead()->asBoundary() : 0 : 0;
  assert(rootBoundary != 0);
  Node *initialChild = rootBoundary->childrenHead();
  assert(initialChild != 0);
  assert(g_dynamicIdentityChildComposeCount == 1);

  {
    loka::core::StateTrackerGuard guard(rootBoundary->tracker());
    propsState.set(true);
  }
  scene.flushInvalidation();
  assert(rootBoundary->childrenHead() == initialChild);
  assert(g_dynamicIdentityChildComposeCount == 1);
  assert(platform.lastFullRebuild_ == false);
  assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_PROPS) != 0);

  {
    loka::core::StateTrackerGuard guard(rootBoundary->tracker());
    layoutState.set(20);
  }
  scene.flushInvalidation();
  assert(rootBoundary->childrenHead() == initialChild);
  assert(g_dynamicIdentityChildComposeCount == 1);
  assert(platform.lastFullRebuild_ == false);
  assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_LAYOUT) != 0);

  {
    loka::core::StateTrackerGuard guard(rootBoundary->tracker());
    childState.set(true);
  }
  scene.flushInvalidation();
  assert(rootBoundary->childrenHead() != 0);
  assert(platform.lastFullRebuild_ == true);
  assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_CHILD) != 0);

  scene.unmount();
  g_dynamicIdentityPropsState = 0;
  g_dynamicIdentityLayoutState = 0;
  g_dynamicIdentityChildState = 0;
}

void testDynamicBoundaryDetachesSubtreeBeforeChildRecompose()
{
  using loka::app::scene::IPlatformController;
  using loka::app::scene::Node;
  using loka::app::scene::NodeDirtyFlags;
  using loka::app::scene::Scene;

  class DummyPlatformController : public IPlatformController
  {
  public:
    DummyPlatformController() : lastMaterialized_(0), lastFlags_(loka::app::scene::NODE_DIRTY_NONE) {}
    virtual void onChange(Node *rootNode, NodeDirtyFlags flags, bool fullRebuild)
    {
      lastMaterialized_ = rootNode;
      lastFlags_ = flags;
    }
    virtual void synchronize() {}
    virtual bool hasPendingSync() const { return false; }
    virtual void destroy() {}

    Node *lastMaterialized_;
    NodeDirtyFlags lastFlags_;
  };

  g_dynamicDetachChildAttachCount = 0;
  g_dynamicDetachChildDetachCount = 0;
  g_dynamicDetachShowChild = true;

  Scene scene(DynamicDetachRootBoundary());
  DummyPlatformController platform;
  scene.mount(&platform);
  scene.updateAttached(true);

  assert(g_dynamicDetachChildAttachCount == 1);
  assert(g_dynamicDetachChildDetachCount == 0);

  g_dynamicDetachShowChild = false;
  scene.invalidate(loka::app::scene::NODE_DIRTY_CHILD);

  assert(g_dynamicDetachChildAttachCount == 1);
  assert(g_dynamicDetachChildDetachCount == 1);
  assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_CHILD) != 0);

  scene.unmount();
  g_dynamicDetachShowChild = true;
}

void testDynamicBoundaryRecomposeDoesNotDuplicateBoundaryCallbacks()
{
  using loka::app::scene::IPlatformController;
  using loka::app::scene::Node;
  using loka::app::scene::NodeDirtyFlags;
  using loka::app::scene::Scene;

  class DummyPlatformController : public IPlatformController
  {
  public:
    DummyPlatformController() : lastMaterialized_(0), lastFlags_(loka::app::scene::NODE_DIRTY_NONE) {}
    virtual void onChange(Node *rootNode, NodeDirtyFlags flags, bool fullRebuild)
    {
      lastMaterialized_ = rootNode;
      lastFlags_ = flags;
    }
    virtual void synchronize() {}
    virtual bool hasPendingSync() const { return false; }
    virtual void destroy() {}

    Node *lastMaterialized_;
    NodeDirtyFlags lastFlags_;
  };

  loka::core::EmitterState emitter;
  g_dynamicCallbackEmitter = &emitter;
  g_dynamicCallbackFireCount = 0;

  Scene scene(DynamicCallbackRootBoundary());
  DummyPlatformController platform;
  scene.mount(&platform);
  scene.updateAttached(true);

  emitter.emit();
  assert(g_dynamicCallbackFireCount == 1);

  scene.invalidate(loka::app::scene::NODE_DIRTY_CHILD);
  emitter.emit();
  assert(g_dynamicCallbackFireCount == 2);

  scene.invalidate(loka::app::scene::NODE_DIRTY_CHILD);
  emitter.emit();
  assert(g_dynamicCallbackFireCount == 3);

  scene.unmount();
  g_dynamicCallbackEmitter = 0;
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
    virtual void onChange(Node *rootNode, NodeDirtyFlags flags, bool fullRebuild)
    {
      (void)rootNode;
      lastFlags_ = flags;
      ++calls_;
    }
    virtual void synchronize() {}
    virtual bool hasPendingSync() const { return false; }
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

void testDynamicBoundaryObservedParentOwnedStateTriggersChildRecompose()
{
  using loka::app::scene::IPlatformController;
  using loka::app::scene::Node;
  using loka::app::scene::NodeDirtyFlags;
  using loka::app::scene::Scene;

  class DirtyCapturePlatformController : public IPlatformController
  {
  public:
    DirtyCapturePlatformController() : calls_(0), lastFlags_(loka::app::scene::NODE_DIRTY_NONE), lastFullRebuild_(false) {}
    virtual void onChange(Node *rootNode, NodeDirtyFlags flags, bool fullRebuild)
    {
      (void)rootNode;
      lastFlags_ = flags;
      lastFullRebuild_ = fullRebuild;
      ++calls_;
    }
    virtual void synchronize() {}
    virtual bool hasPendingSync() const { return false; }
    virtual void destroy() {}

    int calls_;
    NodeDirtyFlags lastFlags_;
    bool lastFullRebuild_;
  };

  g_foreignDynamicObservedComposeCount = 0;
  g_foreignBoundaryObservedState = 0;

  Scene scene(ForeignDynamicObservedHostBoundary());
  DirtyCapturePlatformController platform;
  scene.mount(&platform);
  scene.updateAttached(true);

  assert(g_foreignBoundaryObservedState != 0);
  assert(g_foreignDynamicObservedComposeCount == 1);

  loka::app::scene::BoundaryNode *rootBoundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  assert(rootBoundary != 0);
  {
    loka::core::StateTrackerGuard guard(rootBoundary->tracker());
    g_foreignBoundaryObservedState->set(true);
  }

  scene.flushInvalidation();
  assert(g_foreignDynamicObservedComposeCount >= 2);
  assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_CHILD) != 0);
  assert(platform.lastFullRebuild_ == true);

  scene.unmount();
  g_foreignBoundaryObservedState = 0;
}

void testDynamicBoundaryObservedParentOwnedStateSwapsSampleLikeBranch()
{
  using loka::app::scene::IPlatformController;
  using loka::app::scene::Node;
  using loka::app::scene::NodeDirtyFlags;
  using loka::app::scene::Scene;

  class DirtyCapturePlatformController : public IPlatformController
  {
  public:
    DirtyCapturePlatformController() : lastFlags_(loka::app::scene::NODE_DIRTY_NONE) {}
    virtual void onChange(Node *rootNode, NodeDirtyFlags flags, bool fullRebuild)
    {
      (void)rootNode;
      (void)fullRebuild;
      lastFlags_ = flags;
    }
    virtual void synchronize() {}
    virtual bool hasPendingSync() const { return false; }
    virtual void destroy() {}
    NodeDirtyFlags lastFlags_;
  };

  g_foreignDynamicObservedComposeCount = 0;
  g_foreignBoundaryObservedState = 0;

  Scene scene(SampleLikeHostBoundary());
  DirtyCapturePlatformController platform;
  scene.mount(&platform);
  scene.updateAttached(true);

  loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(root != 0);
  assert(findNodeByTestIdRecursive(root, "DynamicDetailsButton") != 0);
  assert(findNodeByTestIdRecursive(root, "DynamicDetailsText") == 0);

  loka::app::scene::BoundaryNode *rootBoundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  assert(rootBoundary != 0);
  assert(g_foreignBoundaryObservedState != 0);
  {
    loka::core::StateTrackerGuard guard(rootBoundary->tracker());
    g_foreignBoundaryObservedState->set(true);
  }

  scene.flushInvalidation();

  root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(root != 0);
  assert(findNodeByTestIdRecursive(root, "DynamicDetailsButton") == 0);
  assert(findNodeByTestIdRecursive(root, "DynamicDetailsText") != 0);
  assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_CHILD) != 0);
  assert(g_foreignDynamicObservedComposeCount >= 2);

  scene.unmount();
  g_foreignBoundaryObservedState = 0;
}

void testPopupMenuSelectionStateDoesNotInvalidateScene()
{
  using loka::app::scene::IPlatformController;
  using loka::app::scene::Node;
  using loka::app::scene::NodeDirtyFlags;
  using loka::app::scene::Scene;

  class DirtyCapturePlatformController : public IPlatformController
  {
  public:
    DirtyCapturePlatformController() : calls_(0), lastFlags_(loka::app::scene::NODE_DIRTY_NONE) {}
    virtual void onChange(Node *rootNode, NodeDirtyFlags flags, bool fullRebuild)
    {
      (void)rootNode;
      lastFlags_ = flags;
      ++calls_;
    }
    virtual void synchronize() {}
    virtual bool hasPendingSync() const { return false; }
    virtual void destroy() {}

    int calls_;
    NodeDirtyFlags lastFlags_;
  };

  loka::core::MutableState<int> selectedIndex(0);
  loka::core::MutableState<bool> enabled(true);
  g_popupSelectedIndexState = &selectedIndex;
  g_popupEnabledState = &enabled;

  Scene scene(PopupObservedHostBoundary());
  DirtyCapturePlatformController platform;
  scene.mount(&platform);
  scene.updateAttached(true);
  assert(platform.calls_ == 1);

  loka::app::scene::BoundaryNode *boundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  assert(boundary != 0);
  {
    loka::core::StateTrackerGuard guard(boundary->tracker());
    selectedIndex.set(1);
  }

  assert(platform.calls_ == 1);
  assert(scene.flushInvalidation() == false);
  assert(platform.calls_ == 1);

  {
    loka::core::StateTrackerGuard guard(boundary->tracker());
    enabled.set(false);
  }

  assert(platform.calls_ == 2);
  assert((platform.lastFlags_ & loka::app::scene::NODE_DIRTY_PROPS) != 0);

  scene.unmount();
  g_popupSelectedIndexState = 0;
  g_popupEnabledState = 0;
}

void testOpenFileDialogStatesDoNotInvalidateScene()
{
  using loka::app::scene::IPlatformController;
  using loka::app::scene::Node;
  using loka::app::scene::NodeDirtyFlags;
  using loka::app::scene::Scene;

  class DirtyCapturePlatformController : public IPlatformController
  {
  public:
    DirtyCapturePlatformController() : calls_(0), lastFlags_(loka::app::scene::NODE_DIRTY_NONE) {}
    virtual void onChange(Node *rootNode, NodeDirtyFlags flags, bool fullRebuild)
    {
      (void)rootNode;
      lastFlags_ = flags;
      ++calls_;
    }
    virtual void synchronize() {}
    virtual bool hasPendingSync() const { return false; }
    virtual void destroy() {}

    int calls_;
    NodeDirtyFlags lastFlags_;
  };

  loka::core::MutableState<loka::app::FileChooserResult> result(loka::app::FileChooserResult::Canceled());
  loka::core::MutableState<bool> visible(false);
  g_openFileResultState = &result;
  g_openFileVisibleState = &visible;

  Scene scene(OpenFileObservedHostBoundary());
  DirtyCapturePlatformController platform;
  scene.mount(&platform);
  scene.updateAttached(true);
  assert(platform.calls_ == 1);

  loka::app::scene::BoundaryNode *boundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  assert(boundary != 0);
  {
    loka::core::StateTrackerGuard guard(boundary->tracker());
    result.set(loka::app::FileChooserResult::Error(7));
  }

  assert(platform.calls_ == 1);
  assert(scene.flushInvalidation() == false);
  assert(platform.calls_ == 1);

  {
    loka::core::StateTrackerGuard guard(boundary->tracker());
    visible.set(true);
  }

  assert(platform.calls_ == 1);
  assert(scene.flushInvalidation() == false);
  assert(platform.calls_ == 1);

  scene.unmount();
  g_openFileResultState = 0;
  g_openFileVisibleState = 0;
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
    virtual void onChange(Node *rootNode, NodeDirtyFlags flags, bool fullRebuild)
    {
      lastMaterialized_ = rootNode;
      lastFlags_ = flags;
      ++calls_;
    }
    virtual void synchronize() {}
    virtual bool hasPendingSync() const { return false; }
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
    virtual void onChange(Node *rootNode, NodeDirtyFlags flags, bool fullRebuild)
    {
      lastMaterialized_ = rootNode;
      lastFlags_ = flags;
      ++calls_;
    }
    virtual void synchronize() {}
    virtual bool hasPendingSync() const { return false; }
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

void testSceneCompositionDiffMarksChildDirtyAsFullRebuild()
{
  using loka::app::scene::IPlatformController;
  using loka::app::scene::Node;
  using loka::app::scene::NodeDirtyFlags;
  using loka::app::scene::Scene;

  class DirtyCapturePlatformController : public IPlatformController
  {
  public:
    DirtyCapturePlatformController()
        : lastMaterialized_(0), lastFlags_(loka::app::scene::NODE_DIRTY_NONE), lastFullRebuild_(false), calls_(0) {}
    virtual void onChange(Node *rootNode, NodeDirtyFlags flags, bool fullRebuild)
    {
      lastMaterialized_ = rootNode;
      lastFlags_ = flags;
      lastFullRebuild_ = fullRebuild;
      ++calls_;
    }
    virtual void synchronize() {}
    virtual bool hasPendingSync() const { return false; }
    virtual void destroy() {}

    Node *lastMaterialized_;
    NodeDirtyFlags lastFlags_;
    bool lastFullRebuild_;
    int calls_;
  };

  Scene scene(RootBoundary());
  DirtyCapturePlatformController platform;
  scene.mount(&platform);
  scene.updateAttached(true);

  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_LAYOUT);
  assert(scene.compositionDiff().fullRebuild == false);
  scene.flushInvalidation();
  assert(scene.compositionDiff().valid == false);
  assert(platform.lastFullRebuild_ == false);

  scene.requestInvalidate(loka::app::scene::NODE_DIRTY_CHILD);
  assert(scene.compositionDiff().fullRebuild == true);
  scene.flushInvalidation();
  assert(platform.calls_ == 3);
  assert(platform.lastFullRebuild_ == true);

  scene.unmount();
}

void testWindowFlushSceneInvalidationSynchronizesPendingPlatformWork()
{
  using loka::app::scene::IPlatformController;
  using loka::app::scene::Node;
  using loka::app::scene::NodeDirtyFlags;
  using loka::app::scene::Scene;

  class DummyPlatformController : public IPlatformController
  {
  public:
    DummyPlatformController() : lastMaterialized_(0), lastFlags_(loka::app::scene::NODE_DIRTY_NONE), lastFullRebuild_(false), calls_(0) {}
    virtual void onChange(Node *rootNode, NodeDirtyFlags flags, bool fullRebuild)
    {
      lastMaterialized_ = rootNode;
      lastFlags_ = flags;
      lastFullRebuild_ = fullRebuild;
      ++calls_;
    }
    virtual void synchronize() {}
    virtual bool hasPendingSync() const { return false; }
    virtual void destroy() {}

    Node *lastMaterialized_;
    NodeDirtyFlags lastFlags_;
    bool lastFullRebuild_;
    int calls_;
  };

  class TestWindow : public Window
  {
  public:
    explicit TestWindow(Scene *scene)
        : Window(0, WindowProps().scene(scene)), pendingSync_(false), synchronizeCalls_(0) {}

    virtual bool hasPendingScenePlatformSync() const { return pendingSync_; }

    virtual void synchronizeScenePlatform()
    {
      ++synchronizeCalls_;
      pendingSync_ = false;
    }

    bool pendingSync_;
    int synchronizeCalls_;
  };

  Scene scene(RootBoundary());
  DummyPlatformController platform;
  scene.mount(&platform);

  {
    TestWindow window(&scene);
    assert(window.scene() == &scene);

    scene.requestInvalidate(loka::app::scene::NODE_DIRTY_PROPS);
    const bool changed = window.flushSceneInvalidation();
    assert(changed);
    assert(window.synchronizeCalls_ == 1);

    window.pendingSync_ = true;
    const bool changedPendingOnly = window.flushSceneInvalidation();
    assert(changedPendingOnly == false);
    assert(window.synchronizeCalls_ == 2);

    window.sceneManager()->commitTransaction(&scene, 0);
  }

  scene.unmount();
}

void testSceneManagerTransaction()
{
  printf("\n==== [testSceneManagerTransaction/SceneManager] start ====\n");
  using loka::app::scene::Scene;

  class TestWindow : public Window
  {
  public:
    TestWindow() : Window(0, WindowProps()) {}
  };

  Scene *sceneA = new Scene(RootBoundary());
  Scene *sceneB = new Scene(RootBoundary());
  TestWindow window;
  SceneManager *mgr = window.sceneManager();
  assert(mgr != 0);

  assert(mgr->getCurrentScene().get() == 0);
  assert(sceneA->getWindow() == 0);
  assert(sceneB->getWindow() == 0);
  assert(sceneA->getAttachedState()->get() == false);
  assert(sceneB->getAttachedState()->get() == false);
  assert(sceneA->getLifecycleState()->get() == ON_CREATE);
  assert(sceneB->getLifecycleState()->get() == ON_CREATE);

  mgr->commitTransaction(0, sceneA);
  assert(mgr->getCurrentScene().get() == sceneA);
  assert(sceneA->getWindow() == &window);
  assert(sceneA->getAttachedState()->get() == true);
  assert(sceneA->getLifecycleState()->get() == ON_ATTACH);

  mgr->commitTransaction(sceneA, sceneB);
  assert(mgr->getCurrentScene().get() == sceneB);
  assert(sceneA->getWindow() == 0);
  assert(sceneA->getAttachedState()->get() == false);
  assert(sceneA->getLifecycleState()->get() == ON_DETACH);
  assert(sceneB->getWindow() == &window);
  assert(sceneB->getAttachedState()->get() == true);
  assert(sceneB->getLifecycleState()->get() == ON_ATTACH);

  mgr->commitTransaction(sceneB, sceneB);
  assert(mgr->getCurrentScene().get() == sceneB);
  assert(sceneB->getWindow() == &window);
  assert(sceneB->getAttachedState()->get() == true);
  assert(sceneB->getLifecycleState()->get() == ON_ATTACH);

  mgr->commitTransaction(sceneB, 0);
  assert(mgr->getCurrentScene().get() == 0);
  assert(sceneB->getWindow() == 0);
  assert(sceneB->getAttachedState()->get() == false);
  assert(sceneB->getLifecycleState()->get() == ON_DETACH);

  delete sceneA;
  delete sceneB;
  printf("--- [testSceneManagerTransaction/SceneManager] end ---\n");
}

void testSceneManagerPendingTransactionsAndBindings()
{
  printf("\n==== [testSceneManagerPendingTransactionsAndBindings/SceneManager] start ====\n");
  using loka::app::scene::Scene;

  class InspectableSceneManager : public SceneManager
  {
  public:
    SceneTransactionList pendingTransactions() const
    {
      return getPendingTransactions();
    }

    void handleNextPending()
    {
      handleNextTransaction();
    }
  };

  class TestWindow : public Window
  {
  public:
    TestWindow() : Window(0, WindowProps()) {}
  };

  struct ChangeCounter
  {
    static void onChange(void *userData)
    {
      int *count = static_cast<int *>(userData);
      ++(*count);
    }
  };

  Scene *sceneA = new Scene(RootBoundary());
  Scene *sceneB = new Scene(RootBoundary());
  TestWindow window;
  InspectableSceneManager mgr;
  int currentSceneNotifyCount = 0;

  mgr.setWindow(&window);
  assert(mgr.pendingTransactions().empty());
  mgr.handleNextPending();
  assert(mgr.pendingTransactions().empty());

  loka::core::State<Scene *> *currentSceneState =
      const_cast<loka::core::State<Scene *> *>(&mgr.getCurrentScene());
  currentSceneState->bind(&ChangeCounter::onChange, &currentSceneNotifyCount, false);

  mgr.commitTransaction(0, sceneA);
  assert(mgr.pendingTransactions().empty());
  assert(mgr.getCurrentScene().get() == sceneA);
  assert(currentSceneNotifyCount == 1);

  mgr.commitTransaction(sceneA, sceneB);
  assert(mgr.pendingTransactions().empty());
  assert(mgr.getCurrentScene().get() == sceneB);
  assert(currentSceneNotifyCount == 2);

  mgr.commitTransaction(sceneB, sceneB);
  assert(mgr.pendingTransactions().empty());
  assert(mgr.getCurrentScene().get() == sceneB);
  assert(currentSceneNotifyCount == 2);

  mgr.commitTransaction(sceneB, 0);
  assert(mgr.pendingTransactions().empty());
  assert(mgr.getCurrentScene().get() == 0);
  assert(currentSceneNotifyCount == 3);

  currentSceneState->unbind(&ChangeCounter::onChange, &currentSceneNotifyCount);
  delete sceneA;
  delete sceneB;
  printf("--- [testSceneManagerPendingTransactionsAndBindings/SceneManager] end ---\n");
}

void testSceneLifecyclePublishesDestroy()
{
  printf("\n==== [testSceneLifecyclePublishesDestroy/Scene] start ====\n");
  using loka::app::scene::Scene;

  struct LifecycleCapture
  {
    static void onChange(void *userData)
    {
      Capture *capture = static_cast<Capture *>(userData);
      capture->values.push_back(capture->scene->getLifecycleState()->get());
    }

    struct Capture
    {
      Capture() : scene(0), values() {}
      Scene *scene;
      std::vector<SceneLifecycle> values;
    };
  };

  class TestWindow : public Window
  {
  public:
    TestWindow() : Window(0, WindowProps()) {}
  };

  Scene *scene = new Scene(RootBoundary());
  TestWindow window;
  LifecycleCapture::Capture capture;
  capture.scene = scene;
  scene->getLifecycleState()->bind(&LifecycleCapture::onChange, &capture, false);

  window.sceneManager()->commitTransaction(0, scene);
  window.sceneManager()->commitTransaction(scene, 0);
  delete scene;

  assert(capture.values.size() == 3);
  assert(capture.values[0] == ON_ATTACH);
  assert(capture.values[1] == ON_DETACH);
  assert(capture.values[2] == ON_DESTROY);

  printf("--- [testSceneLifecyclePublishesDestroy/Scene] end ---\n");
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

void testStaticButtonAndCellTextAreOwnedPerDefinition()
{
  loka::app::ButtonDefinition firstButton("First");
  loka::app::ButtonDefinition secondButton("Second");
  assert(firstButton.props.text_ != 0);
  assert(secondButton.props.text_ != 0);
  assert(firstButton.props.text_ != secondButton.props.text_);
  assert(firstButton.props.text_->get().equals(loka::core::String::Literal("First")));
  assert(secondButton.props.text_->get().equals(loka::core::String::Literal("Second")));

  loka::app::CellDefinition firstCell("A1");
  loka::app::CellDefinition secondCell("B2");
  assert(firstCell.props.text_ != 0);
  assert(secondCell.props.text_ != 0);
  assert(firstCell.props.text_ != secondCell.props.text_);
  assert(firstCell.props.text_->get().equals(loka::core::String::Literal("A1")));
  assert(secondCell.props.text_->get().equals(loka::core::String::Literal("B2")));
}

void testMenuItemEnabledBoolDoesNotUseSharedStaticState()
{
  loka::app::MenuItemDefinition enabledItem("Enabled");
  enabledItem.enabled(true);
  assert(enabledItem.enabledBindingState() == 0);
  assert(enabledItem.isEnabledInitial() == true);

  loka::app::MenuItemDefinition disabledItem("Disabled");
  disabledItem.enabled(false);
  assert(disabledItem.enabledBindingState() == 0);
  assert(disabledItem.isEnabledInitial() == false);

  assert(enabledItem.equalsStructure(enabledItem));
  assert(disabledItem.equalsStructure(disabledItem));
  assert(!enabledItem.equalsStructure(disabledItem));
}

void testConditionalNodeTeardownAfterOwnedStateIsSafe()
{
  using namespace loka::app;
  using namespace loka::app::scene;

  class ConditionalTeardownRootNode;
  typedef BoundaryPropsFor<ConditionalTeardownRootNode> ConditionalTeardownRootProps;

  class ConditionalTeardownRootNode : public BoundaryNodeFor<ConditionalTeardownRootNode>
  {
  public:
    ConditionalTeardownRootNode(const ConditionalTeardownRootProps &p)
        : BoundaryNodeFor<ConditionalTeardownRootNode>(ConditionalTeardownRootProps(p)),
          showDetails_(),
          details_(Text("Detail"))
    {
    }

    virtual void attachNode(NodeComposition &c)
    {
      c.declareStates().state(showDetails_, true);
    }

    virtual void composeNode(NodeComposition &c)
    {
      c.declare(VStack() << c.showIf(*showDetails_.state(), details_));
    }

  private:
    BoundState<bool> showDetails_;
    TextDefinition details_;
  };

  class DummyPlatformController : public IPlatformController
  {
  public:
    DummyPlatformController() : lastMaterialized_(0), destroyed_(false) {}
    virtual void onChange(Node *rootNode, loka::app::scene::NodeDirtyFlags flags, bool fullRebuild)
    {
      (void)flags;
      (void)fullRebuild;
      lastMaterialized_ = rootNode;
    }
    virtual void synchronize() {}
    virtual bool hasPendingSync() const { return false; }
    virtual void destroy() { destroyed_ = true; }

    Node *lastMaterialized_;
    bool destroyed_;
  };

  Scene *scene = new Scene(BoundaryDefinition<ConditionalTeardownRootProps, ConditionalTeardownRootNode>());
  DummyPlatformController platform;
  scene->mount(&platform);
  scene->updateAttached(true);
  assert(platform.lastMaterialized_ != 0);
  delete scene;
}
