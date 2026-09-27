#include "app/FocusFact.hpp"
enum Field { HEIGHT };
void pin()
{
  loka::app::FocusFact<Field> focus;
  focus.post(HEIGHT);
}
