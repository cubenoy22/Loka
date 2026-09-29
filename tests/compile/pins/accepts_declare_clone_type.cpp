#include "app/nodes/controls/Button.hpp"
#include "app/scene/composition/NodeComposition.hpp"

int main()
{
  loka::app::scene::NodeComposition composition;
  loka::app::scene::NodeDefinitionBase &d = composition.declare(loka::app::Button("x"));
  (void)d;
  return 0;
}
