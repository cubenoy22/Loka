#include "app/scene/state/NodeState.hpp"
int readDerived(loka::app::scene::DerivedNodeState<int> &sum)
{
  sum.set(1);
  return sum.get();
}
