#ifndef LOKA_UTIL_STATE_TRACKER_GUARD_HPP
#define LOKA_UTIL_STATE_TRACKER_GUARD_HPP
#include "core/StateTracker.hpp"
#include "core/Operation.hpp"
#include <cassert>

namespace loka
{
  namespace core
  {
    /** A transaction that joins the active clock when its ledger can enroll.
        Inside a turn the callback is not invoked; owners that need a post-commit
        callback use StandaloneTransactionGuard or observe the State.
        Refused enrollment retains the legacy begin/end bracket. */
    struct StateTrackerGuard
    {
      typedef void (*InvalidateFn)(void *userData);
      StateTracker *tracker;
      InvalidateFn invalidateFn;
      void *invalidateUserData;
      StateTrackerGuard(StateTracker *t, InvalidateFn fn = 0, void *userData = 0)
          : tracker(t),
            invalidateFn(fn),
            invalidateUserData(userData),
            mode_(NONE)
      {
        if (!this->tracker)
          return;
        switch (Operation::openActive(this->tracker))
        {
        case OPEN_OK:
        case OPEN_ALREADY_OPEN:
          this->mode_ = JOINED;
          return;
        case OPEN_REFUSED_BUSY:
        case OPEN_REFUSED_CLOSING:
        case OPEN_NO_CLOCK:
        case OPEN_CLOCK_REFUSED:
        case OPEN_REFUSED_NOT_PUSH:
          this->tracker->begin();
          this->mode_ = OWNED;
        }
      }
      ~StateTrackerGuard()
      {
        if (this->mode_ == OWNED)
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
    private:
      enum Mode { NONE, JOINED, OWNED };
      Mode mode_;
      StateTrackerGuard(const StateTrackerGuard &);
      StateTrackerGuard &operator=(const StateTrackerGuard &);
    };

    /** A bounded preparation transaction that must commit before its owner
        reads the result (commit-before-read). It does not join the clock.
        Owners: menu composition, Scene installation, bootstrap.
        Its begin/end and callback mechanism mirrors the OWNED policy above. */
    struct StandaloneTransactionGuard
    {
      typedef void (*InvalidateFn)(void *userData);
      StateTracker *tracker;
      InvalidateFn invalidateFn;
      void *invalidateUserData;
      StandaloneTransactionGuard(StateTracker *t, InvalidateFn fn = 0, void *userData = 0)
          : tracker(t),
            invalidateFn(fn),
            invalidateUserData(userData)
      {
        if (tracker)
          tracker->begin();
      }
      ~StandaloneTransactionGuard()
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
    private:
      StandaloneTransactionGuard(const StandaloneTransactionGuard &);
      StandaloneTransactionGuard &operator=(const StandaloneTransactionGuard &);
    };

  } // namespace core
} // namespace loka

#endif // LOKA_UTIL_STATE_TRACKER_GUARD_HPP
