#include "app/style/AttributedString.hpp"

void attributedStringPin()
{
  using namespace loka::app;
  const AttributedString value = Styled(loka::core::String("a"), Bold) + Styled("b", Bold);
  (void)value;
}
