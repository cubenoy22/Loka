#include "app/scene/node/ComposableNode.hpp"
class DerivedDependencyPin : public loka::app::scene::ComposableNode
{
  void declare(loka::app::scene::DerivedNodeState<int> &out,
               loka::app::scene::NodeState<int> &input,
               loka::core::DerivedState<int>::EvalFn *eval)
  {
    this->derived(out, input, eval);
  }
};
