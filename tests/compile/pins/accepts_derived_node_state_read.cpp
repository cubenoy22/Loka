#include "app/scene/state/NodeState.hpp"
int readDerived(loka::app::scene::DerivedNodeState<int> &sum)
{
  return sum.get();
}
