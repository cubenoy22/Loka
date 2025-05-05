#include <windows.h>
#include "Win32App.hpp"
#include <iostream>
#include <cassert>
#include "core/State.hpp"
#include "core/Tracker.hpp"

void testDependencyPropagationCases()
{
  std::cout << "testDependencyPropagationCases" << std::endl;
  // --- 多段依存 A→B→C ---
  struct SimpleState : public StateBase
  {
    int value = 0;
    void setValue(int v, StdTracker *tracker)
    {
      value = v;
      if (tracker)
        tracker->markDirty(this);
    }
    bool recompute() override { return true; }
    std::vector<StateBase *> getDependencyStates() const override { return {}; }
  };
  struct DerivedState : public StateBase
  {
    StateBase *dep;
    int value = 0;
    DerivedState(StateBase *d) : dep(d) {}
    void setValue(int v, StdTracker *tracker)
    {
      value = v;
      if (tracker)
        tracker->markDirty(this);
    }
    bool recompute() override
    {
      auto *s = dynamic_cast<SimpleState *>(dep);
      auto *d2 = dynamic_cast<DerivedState *>(dep);
      int old = value;
      if (s)
        value = s->value + 1;
      else if (d2)
        value = d2->value + 1;
      printf("recompute: state=%p, old=%d, new=%d\n", (void *)this, old, value);
      return value != old;
    }
    std::vector<StateBase *> getDependencyStates() const override { return {dep}; }
  };
  SimpleState a;
  DerivedState b(&a);
  DerivedState c(&b);
  std::vector<StateBase *> states = {&a, &b, &c};
  StdTracker tracker(states);
  tracker.begin();
  a.setValue(10, &tracker);
  tracker.end();
  assert(b.value == 11 && c.value == 12);
  tracker.begin();
  a.setValue(20, &tracker);
  tracker.end();
  assert(b.value == 21 && c.value == 22);
  // --- 複数Derivedが1つのStateに依存 ---
  SimpleState s;
  DerivedState d1(&s);
  DerivedState d2(&s);
  std::vector<StateBase *> states2 = {&s, &d1, &d2};
  StdTracker tracker2(states2);
  tracker2.begin();
  s.setValue(5, &tracker2);
  tracker2.end();
  assert(d1.value == 6 && d2.value == 6);
  // --- 依存先の更新時にDerivedが再評価されるか ---
  tracker2.begin();
  s.setValue(42, &tracker2);
  tracker2.end();
  assert(d1.value == 43 && d2.value == 43);
  // --- 依存していないDerivedが影響を受けない ---
  SimpleState s2;
  DerivedState d3(&s2);
  std::vector<StateBase *> states3 = {&s, &d1, &d2, &s2, &d3};
  StdTracker tracker3(states3);
  tracker3.begin();
  s.setValue(100, &tracker3);
  s2.setValue(200, &tracker3);
  tracker3.end();
  assert(d1.value == 101 && d2.value == 101);
  assert(d3.value == 201);
  std::cout << "All dependency propagation tests passed." << std::endl;
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
  return 0;
}
