#include "app/style/AttributedString.hpp"

void attributedStringPin()
{
  using namespace loka::app;
  const AttributedString value = Styled("a", Bold) + BlockStyle();
  (void)value;
}
