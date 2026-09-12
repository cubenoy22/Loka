#include "app/nodes/nestable/Keyed.hpp"

struct NonBoundary
{
  void declare(loka::app::scene::NodeComposition &) {}
};

void refusesNonBoundary(loka::core::State<int> &key)
{
  NonBoundary node;
  loka::app::Keyed(key, &node, &NonBoundary::declare, loka::app::reservation::SeatNodes<loka::app::reservation::End>());
}
