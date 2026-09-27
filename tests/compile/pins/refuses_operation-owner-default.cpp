#define LOKA_PIN_DEFAULT_OWNER
#include "operation_owner.hpp"
void pin(IPlatformController &controller, Node &node)
{
  PinOwner owner(controller, &node, node.getContext());
  PinOwner fixture(testing::controllerlessOperationPhase(), &node, node.getContext());
}
