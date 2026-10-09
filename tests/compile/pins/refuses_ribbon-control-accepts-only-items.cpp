#include "app/nodes/controls/Ribbon.hpp"

void ribbonDeclarationPin()
{
  using namespace loka::app;
  RibbonControl() << Button("x");
}
