#define LOKA_PIN_WITHOUT_CONTROLLER
#include "borrow_owner.hpp"
void pin(IPlatformController &controller, Node &node)
{
  PinOwner owner(controller, &node, node.getContext());
  PinOwner fixture(testing::controllerlessBorrowPhase(), &node, node.getContext());
}
