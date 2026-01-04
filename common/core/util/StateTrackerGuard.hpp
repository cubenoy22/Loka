#ifndef LOKA_UTIL_STATE_TRACKER_GUARD_HPP
#define LOKA_UTIL_STATE_TRACKER_GUARD_HPP
#include "core/StateTracker.hpp"

// StateTracker用RAIIトランザクションガード
struct StateTrackerGuard
{
  declara::core::PushStateTracker *tracker;
  StateTrackerGuard(declara::core::StateTracker *t)
      : tracker(dynamic_cast<declara::core::PushStateTracker *>(t))
  {
    if (tracker)
      tracker->begin();
  }
  ~StateTrackerGuard()
  {
    if (tracker)
      tracker->end();
  }
};

#endif // LOKA_UTIL_STATE_TRACKER_GUARD_HPP
  
