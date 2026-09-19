#include "app/style/Style.hpp"

int main()
{
  loka::app::TextStyle invalid = loka::app::TextStyle() + loka::app::BlockStyle();
  (void)invalid;
  return 0;
}
