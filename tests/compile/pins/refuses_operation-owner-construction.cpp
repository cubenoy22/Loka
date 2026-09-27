#define LOKA_PIN_WITHOUT_CONTROLLER
#include "operation_owner.hpp"
void pin(IPlatformController &controller, Node &node)
{
  PinOwner owner(controller, &node, node.getContext());
  PinOwner fixture(testing::controllerlessOperationPhase(), &node, node.getContext());
}
