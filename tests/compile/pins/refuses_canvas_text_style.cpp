#include "app/nodes/nestable/Canvas.hpp"
#include "app/nodes/Text.hpp"

int main()
{
  loka::app::Canvas invalid = loka::app::Canvas() + loka::app::Bold;
  (void)invalid;
  return 0;
}
