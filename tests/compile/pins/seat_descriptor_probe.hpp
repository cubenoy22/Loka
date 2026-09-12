#include "app/nodes/nestable/Keyed.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
namespace seat_pin
{
  struct Owner : loka::app::scene::BoundaryNode
  {
    virtual void composeWithContext(loka::app::scene::ComponentContext &, loka::app::scene::ComposeEvent) {}
    void declare(loka::app::scene::NodeComposition &) {}
  };
  struct Good : loka::app::scene::Node
  {
  };
  struct Private : private loka::app::scene::Node
  {
  };
  struct Left : loka::app::scene::Node
  {
  };
  struct Right : loka::app::scene::Node
  {
  };
  struct Ambiguous : Left, Right
  {
  };
  struct Incomplete;
  template <class List> void door()
  {
    loka::core::MutableState<int> key(0);
    Owner owner;
    loka::app::Keyed(key, &owner, &Owner::declare, loka::app::reservation::SeatNodes<List>());
  }
} // namespace seat_pin
