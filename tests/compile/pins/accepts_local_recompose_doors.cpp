/**
 * Local-recompose strategy doors are private even to app boundary subclasses.
 * This accepts twin pins that contract together with
 * refuses_local_recompose_doors.cpp, which shares the same header environment.
 */
#include "app/nodes/boundary/RecomposingBoundary.hpp"

using namespace loka::app::scene;
class Probe;
typedef BoundaryPropsFor<Probe> ProbeProps;
class Probe : public RecomposingBoundaryFor<Probe, BoundaryNodeFor<Probe> >
{
public:
  explicit Probe(const ProbeProps &p) : RecomposingBoundaryFor<Probe, BoundaryNodeFor<Probe> >(p) {}
  virtual void composeNode(NodeComposition &c)
  {
    (void)c;
  }
};
