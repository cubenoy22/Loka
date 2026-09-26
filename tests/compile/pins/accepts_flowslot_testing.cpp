#include "app/scene/state/FlowSlot.hpp"
#include "testing/flow/FlowSlotTestAccess.hpp"
void acceptsFlowSlotTesting()
{
  loka::app::scene::FlowSlot<loka::dsl::FlowChain<int, int> > slot(loka::dsl::testing::FlowSlotTestAccess::unowned());
}
