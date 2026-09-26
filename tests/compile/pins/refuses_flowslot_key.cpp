#include "app/scene/state/FlowSlot.hpp"
void refusesFlowSlotKey()
{
  const loka::dsl::testing::UnownedFlowSlotKey key;
  loka::app::scene::FlowSlot<loka::dsl::FlowChain<int, int> > slot(key);
}
