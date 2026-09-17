#include "app/nodes/nestable/Canvas.hpp"
#include "app/nodes/Text.hpp"

int main()
{
  loka::app::TextDefinitionWithAttr valid = loka::app::Text("x") + loka::app::Bold;
  (void)valid;
  return 0;
}
