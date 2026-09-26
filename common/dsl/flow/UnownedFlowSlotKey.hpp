#ifndef LOKA_DSL_FLOW_UNOWNED_FLOW_SLOT_KEY_HPP
#define LOKA_DSL_FLOW_UNOWNED_FLOW_SLOT_KEY_HPP
namespace loka { namespace dsl { namespace testing {
  struct FlowSlotTestAccess;
  /** Testing-only permission for a slot whose holder controls its lifetime. */
  class UnownedFlowSlotKey
  {
  public:
    UnownedFlowSlotKey(const UnownedFlowSlotKey &) {}
  private:
    friend struct FlowSlotTestAccess;
    UnownedFlowSlotKey() {}
  };
} } }
#endif
