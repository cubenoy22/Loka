#include "app/nodes/controls/Ribbon.hpp"

void ribbonDeclarationPin()
{
  using namespace loka::app;
  HStack() << RibbonItem("x");
}
