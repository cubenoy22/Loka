#ifndef LOKA_UTIL_STATE_TRACKER_GUARD_HPP
#define LOKA_UTIL_STATE_TRACKER_GUARD_HPP
#include "loka/core/StateTracker.hpp"

namespace loka
{
  namespace core
  {
    // StateTracker用RAIIトランザクションガード
    struct StateTrackerGuard
    {
      typedef void (*InvalidateFn)(void *userData);
      PushStateTracker *tracker;
      InvalidateFn invalidateFn;
      void *invalidateUserData;
      StateTrackerGuard(StateTracker *t, InvalidateFn fn = 0, void *userData = 0)
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
          if (invalidateFn &&
              tracker->phase() == TRACKER_IDLE &&
              tracker->consumeDirty())
          {
            invalidateFn(invalidateUserData);
          }
        }
      }
    };
  } // namespace core
} // namespace loka

#endif // LOKA_UTIL_STATE_TRACKER_GUARD_HPP
  
