#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/Keyed.hpp"

using namespace loka::app::scene;
class Probe;
typedef BoundaryPropsFor<Probe> ProbeProps;
struct Other
{
  void declare(NodeComposition &) {}
};
class Probe : public BoundaryNodeFor<Probe>
{
public:
  explicit Probe(const ProbeProps &p)
      : BoundaryNodeFor<Probe>(p)
  {
  }
  void declare(NodeComposition &) {}
  virtual void composeNode(NodeComposition &c)
  {
    c.declare(
        loka::app::Keyed(key, this, &Other::declare, loka::app::reservation::SeatNodes<loka::app::reservation::End>()));
  }
  loka::core::MutableState<int> key;
};
