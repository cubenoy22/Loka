#include "core/StateTracker.hpp"

void acceptsTrackerConstruction(const loka::core::PushStateTracker::StateList &states)
{
  loka::core::PushStateTracker empty;
  loka::core::PushStateTracker populated(states);
}
