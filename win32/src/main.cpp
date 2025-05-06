#include <windows.h>
#include "Win32App.hpp"
#include <iostream>
#include <cassert>
#include "core/State.hpp"
#include "core/Tracker.hpp"
#include "core/Scene.hpp"
#include "app/Button.hpp"
#include "app/Renderer.hpp"
#include "core/App.hpp"
#include "core/Window.hpp"
#include <string>

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
  StdTracker tracker(states);
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
  StdTracker tracker2(states2);
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
  StdTracker tracker3(states3);
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
  StdTracker tracker({&s_int, &doubleProp});
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
  StdTracker tracker({&s_int, &doubleProp});
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
  StdTracker tracker({&name, &isValid});
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

struct AutoTransactionGuard
{
  StdTracker *tracker;
  AutoTransactionGuard(StdTracker *t) : tracker(t) { tracker->begin(); }
  ~AutoTransactionGuard() { tracker->end(); }
};

void testBatchTransaction()
{
  printf("\n==== [testBatchTransaction] start ====\n");
  MutableState<int> s1(1);
  MutableState<int> s2(2);
  DerivedState<int> sumProp({&s1}, [&]()
                            { return s1.get() * 2; });
  StdTracker tracker({&s1, &s2, &sumProp});
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
  StdTracker tracker({&s, &doubleProp});
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
  StdTracker tracker(std::vector<StateBase *>(deps2, deps2 + 5));
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

// --- Win32Window/Win32App設計に基づき、WinMainを最小構成に整理 ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  Win32App app(hInstance, nCmdShow);
  app.run();
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
  return 0;
}
