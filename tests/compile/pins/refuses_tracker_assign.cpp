#include "core/StateTracker.hpp"

void refusesTrackerAssignment(loka::core::PushStateTracker &destination,
                              const loka::core::PushStateTracker &source)
{
  destination = source;
}
