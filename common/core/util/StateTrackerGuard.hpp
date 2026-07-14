#ifndef LOKA_UTIL_STATE_TRACKER_GUARD_HPP
#define LOKA_UTIL_STATE_TRACKER_GUARD_HPP
#include "core/StateTracker.hpp"
#include <cassert>

namespace loka
{
  namespace core
  {
    // RAII transaction guard for StateTracker.
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
          bool settled = tracker->end();
          assert(settled && "StateTracker transaction did not settle");
          if (settled && invalidateFn && tracker->phase() == TRACKER_IDLE && tracker->transactionDirty())
          {
            invalidateFn(invalidateUserData);
          }
        }
      }
    };
  } // namespace core
} // namespace loka

#endif // LOKA_UTIL_STATE_TRACKER_GUARD_HPP
