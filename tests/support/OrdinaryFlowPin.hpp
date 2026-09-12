#ifndef LOKA_TESTS_SUPPORT_ORDINARY_FLOW_PIN_HPP
#define LOKA_TESTS_SUPPORT_ORDINARY_FLOW_PIN_HPP
#include "dsl/flow/Flow.hpp"

namespace ordinary_flow_pin
{
  struct MultiplyByTwo
  {
    typedef int In;
    typedef int Out;
    loka::dsl::StepRunStatus run(const int &in, int &out, loka::dsl::FlowError &) const
    {
      out = in * 2;
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }
  };

  // Shared by the generic-tracker contract and the upstream ordinary-path pin.
  inline loka::dsl::FlowChain<int, int> make(int *input, loka::core::MutableState<int> *result,
                                            loka::core::StateTracker *tracker)
  {
    return loka::dsl::Flow() | loka::dsl::Step(1, MultiplyByTwo()).input(input).onSuccess(result, tracker);
  }
}
#endif
