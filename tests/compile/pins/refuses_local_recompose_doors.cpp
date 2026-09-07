/**
 * Local-recompose strategy doors are private even to app boundary subclasses.
 * This refuses twin pins that contract together with
 * accepts_local_recompose_doors.cpp, which shares the same header environment.
 */
#include "app/nodes/boundary/RecomposingBoundary.hpp"

using namespace loka::app::scene;
class Probe;
typedef BoundaryPropsFor<Probe> ProbeProps;
class Probe : public RecomposingBoundaryFor<Probe, BoundaryNodeFor<Probe> >
{
public:
  explicit Probe(const ProbeProps &p) : RecomposingBoundaryFor<Probe, BoundaryNodeFor<Probe> >(p) {}
  // Compile-only: never invoked. A boundary subclass must not reach the
  // kernel's recompose door directly.
  void probe(ComponentContext &ctx)
  {
    this->recomposeLocalCompositionWithFullFallback(
        ctx, COMPOSE_EVENT_UPDATE, this->LOCAL_RECOMPOSE_APPLY_DIFF_WITH_RETAIN_FAST_PATHS);
  }
};
