#ifndef LOKA_UTIL_STATE_TRACKER_GUARD_HPP
#define LOKA_UTIL_STATE_TRACKER_GUARD_HPP
#include "core/StateTracker.hpp"
#include <cassert>

namespace loka
{
  namespace core
  {
    // RAII transaction guard for any StateTracker. The transaction itself goes
    // through the abstract port, so a non-push tracker (a preparation-scope
    // port, a test double) still brackets its writes with begin()/end(); only
    // the optional invalidate callback needs the push tracker's dirt query.
    struct StateTrackerGuard
    {
      typedef void (*InvalidateFn)(void *userData);
      StateTracker *tracker;
      InvalidateFn invalidateFn;
      void *invalidateUserData;
      StateTrackerGuard(StateTracker *t, InvalidateFn fn = 0, void *userData = 0)
          : tracker(t),
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
          if (settled && invalidateFn && tracker->phase() == TRACKER_IDLE)
          {
            PushStateTracker *push = tracker->asPushTracker();
            if (push && push->transactionDirty())
              invalidateFn(invalidateUserData);
          }
        }
      }
    };
  } // namespace core
} // namespace loka

#endif // LOKA_UTIL_STATE_TRACKER_GUARD_HPP
