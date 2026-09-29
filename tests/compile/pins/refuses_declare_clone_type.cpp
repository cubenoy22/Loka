#include "app/nodes/controls/Button.hpp"
#include "app/scene/composition/NodeComposition.hpp"

int main()
{
  loka::app::scene::NodeComposition composition;
  composition.declare(loka::app::Button("x")).testId("ok");
  return 0;
}
