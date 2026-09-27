#include "app/Focus.hpp"
enum Field { HEIGHT };
void pin()
{
  loka::app::Focus<Field> focus;
  focus.post(HEIGHT);
}
