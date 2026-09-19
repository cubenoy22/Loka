#include "app/style/AttributedString.hpp"

void attributedStringPin()
{
  using namespace loka::app;
  const AttributedString value = loka::core::String("a") + Styled("b", Bold);
  (void)value;
}
