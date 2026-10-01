#include "borrow_owner.hpp"
void pin(IPlatformController &controller, Node &node)
{
  PinOwner owner(controller, &node, node.getContext());
  PinOwner copy(owner);
  (void)copy;
}
