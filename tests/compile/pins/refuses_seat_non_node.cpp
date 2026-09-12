#include "seat_descriptor_probe.hpp"
using namespace loka::app::reservation;
void probe()
{
  seat_pin::door<Nodes<int, 1> >();
}
