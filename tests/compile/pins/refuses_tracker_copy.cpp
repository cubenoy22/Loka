#include "core/StateTracker.hpp"

void refusesTrackerCopy(const loka::core::PushStateTracker &source)
{
  loka::core::PushStateTracker copy(source);
}
