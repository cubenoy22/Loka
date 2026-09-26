#include "testing/core/StateTrackerTestAccess.hpp"

struct ComponentLike
{
  void probe(loka::core::StateTracker &tracker, void (*fn)(void *), void *data)
  {
    loka::core::testing::PushStateTrackerTestAccess::defer(tracker, fn, data);
  }
};
