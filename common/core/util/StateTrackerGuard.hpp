#ifndef LOKA_UTIL_STATE_TRACKER_GUARD_HPP
#define LOKA_UTIL_STATE_TRACKER_GUARD_HPP
#include "core/StateTracker.hpp"

// StateTracker用RAIIトランザクションガード
struct StateTrackerGuard
{
  typedef void (*InvalidateFn)(void *userData);
  loka::core::PushStateTracker *tracker;
  InvalidateFn invalidateFn;
  void *invalidateUserData;
  StateTrackerGuard(loka::core::StateTracker *t, InvalidateFn fn = 0, void *userData = 0)
      : tracker(t ? t->asPushTracker() : 0),
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
  
