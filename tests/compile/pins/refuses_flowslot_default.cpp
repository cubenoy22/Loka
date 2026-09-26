#include "app/scene/state/FlowSlot.hpp"
void refusesFlowSlotDefault()
{
  loka::app::scene::FlowSlot<loka::dsl::FlowChain<int, int> > slot;
}
