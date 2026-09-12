#include "seat_descriptor_probe.hpp"
void probe()
{
  loka::core::MutableState<int> key(0);
  seat_pin::Owner owner;
  loka::app::Keyed(key, &owner, &seat_pin::Owner::declare, 0);
}
