#include <windows.h>
#include <string>
#include <iostream>
#include <cassert>
#include "Win32App.hpp"
#include "core/State.hpp"
#include "core/StateTracker.hpp"
#include "core/Scene.hpp"
#include "app/Button.hpp"
#include "core/PlatformContext.hpp"
#include "Win32PlatformContext.hpp"
#include "core/App.hpp"
#include "core/Window.hpp"
#include "core/SceneManager2.hpp"
#include "core/util/ScopedPtr.hpp"
#include "core/AppConfigurable.hpp"
#include "core/AutoTransactionGuard.hpp"

void testDependencyPropagationCases()
{
  printf("\n==== [testDependencyPropagationCases] start ====\n");
  std::cout << "testDependencyPropagationCases" << std::endl;
  // --- 多段依存 A→B→C ---
  MutableState<int> a;
  struct DerivedState : public StateBase
  {
    StateBase *dep;
    int value = 0;
    DerivedState(StateBase *d) : dep(d) {}
    bool recompute() override
    {
      auto *s = dynamic_cast<MutableState<int> *>(dep);
      auto *d2 = dynamic_cast<DerivedState *>(dep);
      int old = value;
      if (s)
        value = s->get() + 1;
      else if (d2)
        value = d2->value + 1;
      printf("recompute: state=%p, old=%d, new=%d\n", (void *)this, old, value);
      return value != old;
    }
    std::vector<StateBase *> getDependencyStates() const override { return {dep}; }
  };
  DerivedState b(&a);
  DerivedState c(&b);
  std::vector<StateBase *> states = {&a, &b, &c};
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
  std::vector<StateBase *> states2 = {&s, &d1, &d2};
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
  std::vector<StateBase *> states3 = {&s, &d1, &d2, &s2, &d3};
  PushStateTracker tracker3(states3);
  tracker3.begin();
  s.set(100);
  s2.set(200);
  tracker3.end();
  printf("[TEST] 依存分離: d1=%d, d2=%d, d3=%d (期待値: 101, 101, 201)\n", d1.value, d2.value, d3.value);
  assert(d1.value == 101 && d2.value == 101);
  assert(d3.value == 201);
  std::cout << "All dependency propagation tests passed." << std::endl;
  printf("==== [testDependencyPropagationCases] end ====\n");
}

static int doubleFn(const int &v) { return v * 2; }

void testTrackerPropagation()
{
  printf("\n==== [testTrackerPropagation] start ====\n");
  MutableState<int> s_int(10);
  DerivedState<int> doubleProp({&s_int}, [&]()
                               { return s_int.get() * 2; });
  printf("[test] s_int=%p, doubleProp=%p\n", (void *)&s_int, (void *)&doubleProp);
  auto deps = doubleProp.getDependencyStates();
  for (size_t i = 0; i < deps.size(); ++i)
  {
    printf("[test] doubleProp.getDependencyStates()[%zu]=%p\n", i, (void *)deps[i]);
  }
  PushStateTracker tracker({&s_int, &doubleProp});
  // ...既存のデバッグ出力...
  printf("[before begin] s_int=%d, doubleProp=%d\n", s_int.get(), doubleProp.get());
  tracker.begin();
  printf("[after begin] s_int=%d, doubleProp=%d\n", s_int.get(), doubleProp.get());
  s_int.set(21);
  printf("[after set] s_int=%d, doubleProp=%d\n", s_int.get(), doubleProp.get());
  tracker.end();
  printf("[after end] s_int=%d, doubleProp=%d\n", s_int.get(), doubleProp.get());

  assert(s_int.get() == 21);
  assert(doubleProp.get() == 42);
  std::cout << "s_int: " << s_int.get() << std::endl;
  std::cout << "doubleProp: " << doubleProp.get() << std::endl;
  printf("==== [testTrackerPropagation] end ====\n");
}

void testDeferredSideEffect()
{
  printf("\n==== [testDeferredSideEffect] start ====\n");
  MutableState<int> s_int(5);
  DerivedState<int> doubleProp({&s_int}, [&]()
                               { return s_int.get() * 2; });
  PushStateTracker tracker({&s_int, &doubleProp});
  struct DeferredCallback
  {
    static void onDeferred(void *)
    {
      std::cout << "[deferred] UI redraw or log: commit completed!" << std::endl;
    }
  };
  tracker.defer(DeferredCallback::onDeferred, NULL);
  tracker.begin();
  s_int.set(7);
  tracker.end();
  assert(s_int.get() == 7);
  assert(doubleProp.get() == 14);
  std::cout << "s_int: " << s_int.get() << std::endl;
  std::cout << "doubleProp: " << doubleProp.get() << std::endl;
  printf("==== [testDeferredSideEffect] end ====\n");
}

void testTextInputOnChange()
{
  printf("\n==== [testTextInputOnChange] start ====\n");
  MutableState<std::string> name("");
  DerivedState<bool> isValid({&name}, [&]()
                             { return name.get().length() >= 3; });
  PushStateTracker tracker({&name, &isValid});
  struct ValidCallback
  {
    static void onChange(void *userData)
    {
      DerivedState<bool> *isValid = (DerivedState<bool> *)userData;
      std::cout << "[isValid] changed: " << (isValid->get() ? "OK" : "NG") << std::endl;
    }
  };
  isValid.bind(ValidCallback::onChange, &isValid, false);
  std::cout << "[TextInput] name.set(\"ab\")" << std::endl;
  tracker.begin();
  name.set("ab");
  tracker.end();
  assert(isValid.get() == false);
  std::cout << "isValid: " << (isValid.get() ? "OK" : "NG") << std::endl;
  std::cout << "[TextInput] name.set(\"senko\")" << std::endl;
  tracker.begin();
  name.set("senko");
  tracker.end();
  assert(isValid.get() == true);
  std::cout << "isValid: " << (isValid.get() ? "OK" : "NG") << std::endl;
  printf("==== [testTextInputOnChange] end ====\n");
}

void testBatchTransaction()
{
  printf("\n==== [testBatchTransaction] start ====\n");
  MutableState<int> s1(1);
  MutableState<int> s2(2);
  DerivedState<int> sumProp({&s1}, [&]()
                            { return s1.get() * 2; });
  PushStateTracker tracker({&s1, &s2, &sumProp});
  tracker.begin();
  s1.set(10);
  s2.set(20);
  tracker.end();
  assert(s1.get() == 10);
  assert(s2.get() == 20);
  assert(sumProp.get() == 20);
  std::cout << "[Batch] s1=" << s1.get() << ", s2=" << s2.get() << ", sumProp=" << sumProp.get() << std::endl;
  printf("==== [testBatchTransaction] end ====\n");
}

void testRAIITransaction()
{
  printf("\n==== [testRAIITransaction] start ====\n");
  MutableState<int> s(0);
  DerivedState<int> doubleProp({&s}, [&]()
                               { return s.get() * 2; });
  PushStateTracker tracker({&s, &doubleProp});
  {
    AutoTransactionGuard _(&tracker);
    s.set(50);
  }
  assert(s.get() == 50);
  assert(doubleProp.get() == 100);
  std::cout << "[RAII] s=" << s.get() << ", doubleProp=" << doubleProp.get() << std::endl;
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
  std::vector<StateBase *> deps = {&name, &email, &age, &agree};
  DerivedState<bool> isValid(
      deps,
      [&]()
      {
        FormInputs f = {name.get(), email.get(), age.get(), agree.get()};
        return !f.name.empty() && !f.email.empty() && f.age >= 18 && f.agree;
      });
  StateBase *deps2[5];
  for (size_t i = 0; i < deps.size(); ++i)
    deps2[i] = deps[i];
  deps2[deps.size()] = &isValid;
  PushStateTracker tracker(std::vector<StateBase *>(deps2, deps2 + 5));
  struct Callback
  {
    static void onChange(bool v, void *)
    {
      std::cout << "[isValid] changed: " << v << std::endl;
    }
  };
  isValid.bind((StateBase::OnChangeFn)Callback::onChange, &isValid, false);
  tracker.begin();
  name.set("senko");
  email.set("");
  age.set(20);
  agree.set(true);
  tracker.end();
  assert(isValid.get() == false); // emailが空

  tracker.begin();
  email.set("senko@ai");
  age.set(15);
  tracker.end();
  assert(isValid.get() == false); // ageが未満

  tracker.begin();
  age.set(22);
  agree.set(false);
  tracker.end();
  assert(isValid.get() == false); // agreeがfalse

  tracker.begin();
  agree.set(true);
  tracker.end();
  assert(isValid.get() == true); // 全てOK
  printf("==== [testDerivedStruct] end ====\n");
}

// Test dummy Scene (English output for console compatibility)
class TestSceneA : public Scene
{
public:
  TestSceneA() : Scene(new SceneHost()), discardable(true) {}
  void compose(SceneBuilder &b)
  {
    b.Text("[A] This is Test Scene A");
  }
  // bool isDiscardable() const { return discardable; }
  // void requestDiscard(DiscardCallback *cb)
  // {
  //   std::cout << "[A] Save dialog: Discard OK (auto)\n";
  //   if (cb)
  //     (*cb)(true); // Always OK
  // }
  void onAttach() { std::cout << "[A] onAttach\n"; }
  void onDetach() { std::cout << "[A] onDetach\n"; }
  void onDiscardRequestAborted() { std::cout << "[A] onDiscardRequestAborted (emergency save!)\n"; }
  bool discardable;
};

class TestSceneB : public Scene
{
public:
  TestSceneB() : Scene(new SceneHost()), discardable(false), discardRequested(false) {}
  void compose(SceneBuilder &b)
  {
    b.Text("[B] This is Test Scene B");
  }
  // bool isDiscardable() const { return discardable; }
  // void requestDiscard(DiscardCallback *cb)
  // {
  //   std::cout << "[B] Save dialog: Discard not allowed → 1st: NG, 2nd: OK\n";
  //   if (!discardRequested)
  //   {
  //     discardRequested = true;
  //     if (cb)
  //       (*cb)(false); // 1st: NG
  //   }
  //   else
  //   {
  //     discardable = true; // 2nd以降はdiscardableに
  //     if (cb)
  //       (*cb)(true); // 2nd: OK
  //   }
  // }
  void onAttach() { std::cout << "[B] onAttach\n"; }
  void onDetach() { std::cout << "[B] onDetach\n"; }
  void onDiscardRequestAborted() { std::cout << "[B] onDiscardRequestAborted (emergency save!)\n"; }
  bool discardable;
  bool discardRequested;
};

void testSceneManagerTransaction()
{
  std::cout << "\n==== [testSceneManagerTransaction/SceneManager2] start ====\n";
  // PlatformContext *dummyContext = 0;
  // Window win(dummyContext, 0, "テストウィンドウ");
  // SceneManager *mgr = win.sceneManager();
  // mgr->setWindow(&win);

  // SceneManager2を直接使う
  SceneManager2 mgr;

  // TestSceneA/Bは1インスタンスずつ使い回す
  TestSceneA *sceneA = new TestSceneA();
  TestSceneB *sceneB = new TestSceneB();

  // --- 共通で使うテスト用Scene定義 ---
  struct TestSceneB_Abort : public TestSceneB
  {
    bool aborted = false;
    // void onDiscardRequestAborted() override
    // {
    //   aborted = true;
    //   std::cout << "[B] onDiscardRequestAborted (aborted by override)\n";
    // }
  };
  TestSceneB_Abort *sceneB_abort;

  std::cout << "--- シーンAへ ---\n";
  mgr.commitTransaction(0, sceneA);
  assert(mgr.getCurrentScene().get() == sceneA);

  std::cout << "--- シーンBへ ---\n";
  mgr.commitTransaction(sceneA, sceneB);
  assert(mgr.getCurrentScene().get() == sceneB);

  // std::cout << "--- シーンAへ（Bの破棄不可→2回目でOK）---\n";
  // mgr.commitTransaction(sceneB, sceneA);   // 1回目（NG）
  // assert(mgr.getCurrentScene() == sceneB); // まだBのまま
  // mgr.commitTransaction(sceneB, sceneA);   // 2回目（OK）
  // assert(mgr.getCurrentScene() == sceneA); // ここでAに遷移

  // 連打シミュレーション（常に同じsceneBを使い回す）
  std::cout << "--- 連打シミュレーション ---\n";
  for (int i = 0; i < 3; ++i)
  {
    mgr.commitTransaction(mgr.getCurrentScene().get(), sceneB);
    assert(mgr.getCurrentScene().get() == sceneB);
  }

  // --- discardRequest中にcommitTransaction（強制キャンセル）テスト ---
  // SceneManager2はデリゲート/割り込み未実装のためスキップ
  /*
  std::cout << "--- discardRequest中にcommitTransaction（強制キャンセル）テスト ---\n";
  sceneB_abort = new TestSceneB_Abort();
  std::cout << "[test] Created sceneB_abort=" << sceneB_abort << std::endl;
  mgr.commitTransaction(sceneA, sceneB_abort); // Bへ
  assert(mgr.getCurrentScene() == sceneB_abort);
  mgr.commitTransaction(sceneB_abort, sceneA); // Aへ（1回目: NG）
  // すぐにさらにBへ（強制キャンセル）
  std::cout << "[test] Before force cancel: sceneB_abort=" << sceneB_abort << ", aborted=" << sceneB_abort->aborted << std::endl;
  mgr.commitTransaction(sceneA, sceneB_abort); // 強制キャンセル
  std::cout << "[test] After force cancel: sceneB_abort=" << sceneB_abort << ", aborted=" << sceneB_abort->aborted << std::endl;
  // onDiscardRequestAbortedが呼ばれたか検証
  assert(sceneB_abort->aborted);
  */

  // --- 2回連続でfalse（常にNG）テスト ---
  // std::cout << "--- 2回連続でfalse（常にNG）テスト ---\n";
  // struct TestSceneB_AlwaysNG : public TestSceneB
  // {
  //   bool isDiscardable() const override { return false; }
  //   void requestDiscard(DiscardCallback *cb) override
  //   {
  //     std::cout << "[B] Save dialog: Always NG\n";
  //     if (cb)
  //       (*cb)(false);
  //   }
  // };
  // TestSceneB_AlwaysNG *sceneB_ng = new TestSceneB_AlwaysNG();
  // mgr.commitTransaction(sceneA, sceneB_ng);
  // assert(mgr.getCurrentScene() == sceneB_ng);
  // mgr.commitTransaction(sceneB_ng, sceneA);   // 1回目: NG
  // assert(mgr.getCurrentScene() == sceneB_ng); // まだBのまま
  // mgr.commitTransaction(sceneB_ng, sceneA);   // 2回目: NG
  // assert(mgr.getCurrentScene() == sceneB_ng); // まだBのまま

  // --- delegate未設定・割り込みcommitTransactionテスト ---
  // SceneManager2はデリゲート未実装のためスキップ
  /*
  std::cout << "--- delegate未設定: 割り込みcommitTransactionは即座に遷移しない ---\n";
  TestSceneB *sceneB_pending = new TestSceneB();
  mgr.commitTransaction(sceneA, sceneB_pending); // Bへ
  assert(mgr.getCurrentScene() == sceneB_pending);
  mgr.commitTransaction(sceneB_pending, sceneA); // 1回目: NG
  assert(mgr.getCurrentScene() == sceneB_pending); // まだBのまま
  mgr.commitTransaction(sceneB_pending, sceneA); // 2回目: OK
  assert(mgr.getCurrentScene() == sceneA); // ここでAに遷移
  */

  // --- shouldOverridePending=false/true delegateテスト ---
  // SceneManager2はデリゲート未実装のためスキップ
  /*
  std::cout << "--- shouldOverridePending=false delegate: 割り込みcommitTransactionは即座に遷移しない ---\n";
  std::cout << "--- shouldOverridePending=true delegate: 強制キャンセルが即時発動する ---\n";
  */

  std::cout << "--- [testSceneManagerTransaction/SceneManager2] end ---\n";
}

class MyAppConfig : public AppConfigurable
{
public:
  MyAppConfig(PlatformContext *ctx)
      : AppConfigurable(ctx) {}
  void configure(AppBuilder &builder)
  {
    builder.Window(
        WindowOptions()
            .setTitle("DEVELOPERS!")
            .setVisibility(true));
  }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  Win32PlatformContext platformContext;
  MyAppConfig config(&platformContext);
  ScopedPtr<App>(platformContext.createApp(&config, hInstance, nCmdShow))
      ->run();
  return 0;
}

int main()
{
  testDependencyPropagationCases();
  testTrackerPropagation();
  testDeferredSideEffect();
  testBatchTransaction();
  testRAIITransaction();
  testTextInputOnChange();
  testDerivedStruct();
  testSceneManagerTransaction();
  return 0;
}
