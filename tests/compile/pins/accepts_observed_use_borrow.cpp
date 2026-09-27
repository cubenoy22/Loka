#include "app/scene/Node.hpp"
void borrowObservedUse()
{
  loka::app::scene::ObservedUse original;
  loka::app::scene::ObservedUse &borrowed = original;
  (void)borrowed;
}
