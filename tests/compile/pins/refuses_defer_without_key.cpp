#include "testing/core/StateTrackerTestAccess.hpp"

struct ComponentLike
{
  void probe(loka::core::StateTracker &tracker, void (*fn)(void *), void *data)
  {
    tracker.defer(fn, data);
  }
};
