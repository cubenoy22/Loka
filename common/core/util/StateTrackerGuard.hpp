#ifndef LOKA_UTIL_STATE_TRACKER_GUARD_HPP
#define LOKA_UTIL_STATE_TRACKER_GUARD_HPP
#include "core/StateTracker.hpp"

// StateTracker用RAIIトランザクションガード
struct StateTrackerGuard
{
  typedef void (*InvalidateFn)(void *userData);
  declara::core::PushStateTracker *tracker;
  InvalidateFn invalidateFn;
  void *invalidateUserData;
  StateTrackerGuard(declara::core::StateTracker *t, InvalidateFn fn = 0, void *userData = 0)
      : tracker(dynamic_cast<declara::core::PushStateTracker *>(t)),
        invalidateFn(fn),
        invalidateUserData(userData)
  {
    if (tracker)
      tracker->begin();
  }
  ~StateTrackerGuard()
  {
    if (tracker)
    {
      tracker->end();
      if (invalidateFn && tracker->consumeDirty())
      {
        invalidateFn(invalidateUserData);
      }
    }
  }
};

#endif // LOKA_UTIL_STATE_TRACKER_GUARD_HPP
  
