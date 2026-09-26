#include "app/scene/state/FlowSlot.hpp"
void acceptsFlowSlotOwner(loka::app::scene::ComposableNode &owner)
{
  loka::app::scene::FlowSlot<loka::dsl::FlowChain<int, int> > slot(owner);
}
