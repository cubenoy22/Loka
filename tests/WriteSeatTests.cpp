#include "WriteSeatTests.hpp"
#include "support/TestVerify.hpp"
#include "app/scene/state/NodeState.hpp"
#include "app/nodes/controls/EditText.hpp"
#include "app/nodes/controls/PopupMenu.hpp"
#include "core/State.hpp"
#include "core/StateTracker.hpp"
#include "core/util/StateTrackerGuard.hpp"

using namespace loka::core;
using namespace loka::app;
using namespace loka::app::scene;
namespace { struct Count { int calls; Count() : calls(0) {} static void hit(void *p) { ++static_cast<Count *>(p)->calls; } }; }

void testWriteSeatSettlesIdleOwnerTracker()
{
  MutableState<int> value(0); PushStateTracker tracker; tracker.addState(&value);
  NodeState<int> state(&value, &tracker); Count count; value.bind(&Count::hit, &count, false);
  state.writeSeat().set(2, true);
  LOKA_VERIFY(value.get() == 2 && count.calls == 1 && tracker.phase() == TRACKER_IDLE);
}
void testWriteSeatJoinsOpenTracker()
{
  MutableState<int> value(0); PushStateTracker tracker; tracker.addState(&value);
  NodeState<int> state(&value, &tracker); Count count; value.bind(&Count::hit, &count, false);
  { StateTrackerGuard guard(&tracker); state.writeSeat().set(2, true); LOKA_VERIFY(count.calls == 1); }
  LOKA_VERIFY(count.calls == 1 && tracker.phase() == TRACKER_IDLE);
}
void testWriteSeatRawAndInvalid()
{
  MutableState<int> value(0); Count count; value.bind(&Count::hit, &count, false);
  WriteSeat<int>(&value).set(1, true); LOKA_VERIFY(value.get() == 1 && count.calls == 1);
  WriteSeat<int> invalid; invalid.set(2, true); LOKA_VERIFY(value.get() == 1);
}
void testWriteSeatPropsIdentity()
{
  MutableState<int> value(0); PushStateTracker a; PushStateTracker b; NodeState<int> left(&value, &a), right(&value, &b);
  PopupMenuProps one; one.selectedIndex(left); PopupMenuProps two; two.selectedIndex(right);
  LOKA_VERIFY(!(one < two) && !(two < one));
}
