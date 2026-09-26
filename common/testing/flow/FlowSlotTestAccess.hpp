#ifndef LOKA_TESTING_FLOW_FLOW_SLOT_TEST_ACCESS_HPP
#define LOKA_TESTING_FLOW_FLOW_SLOT_TEST_ACCESS_HPP
#include "app/scene/state/FlowSlot.hpp"
namespace loka { namespace dsl { namespace testing {
  /** Also available to scenario drivers compiled without TEST_BUILD. */
  struct FlowSlotTestAccess
  {
    static UnownedFlowSlotKey unowned() { return UnownedFlowSlotKey(); }
    /** Observes enrollment without extending the node's lifetime. */
    template <typename FlowT>
    static const app::scene::ComposableNode *owner(const app::scene::FlowSlot<FlowT> &slot)
    {
      return slot.owner_;
    }
  };
} } }
#endif
