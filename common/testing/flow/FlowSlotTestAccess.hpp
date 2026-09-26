#ifndef LOKA_TESTING_FLOW_FLOW_SLOT_TEST_ACCESS_HPP
#define LOKA_TESTING_FLOW_FLOW_SLOT_TEST_ACCESS_HPP
#include "dsl/flow/UnownedFlowSlotKey.hpp"
namespace loka { namespace dsl { namespace testing {
  /** Also available to scenario drivers compiled without TEST_BUILD. */
  struct FlowSlotTestAccess
  {
    static UnownedFlowSlotKey unowned() { return UnownedFlowSlotKey(); }
  };
} } }
#endif
