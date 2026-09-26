#ifndef LOKA_TESTING_CORE_STATE_TRACKER_TEST_ACCESS_HPP
#define LOKA_TESTING_CORE_STATE_TRACKER_TEST_ACCESS_HPP

#include "core/StateTracker.hpp"

namespace loka
{
  namespace core
  {
    namespace testing
    {
      /** Test-only access to walk identity and tracker-owned registration rows. */
      struct PushStateTrackerTestAccess
      {
        /** Counts actual invalidate deliveries while preserving their production route.
            The probe must outlive the tracker it instruments. */
        struct InvalidationProbe
        {
          InvalidationProbe() : calls(0), callback(0), data(0) {}
          void install(PushStateTracker &tracker)
          {
            this->callback = tracker.invalidateFn_;
            this->data = tracker.invalidateUserData_;
            tracker.setInvalidateCallback(&InvalidationProbe::invoke, this);
          }
          int calls;
        private:
          PushStateTracker::InvalidateFn callback;
          void *data;
          static void invoke(void *data)
          {
            InvalidationProbe *self = static_cast<InvalidationProbe *>(data);
            ++self->calls;
            if (self->callback) self->callback(self->data);
          }
        };

        static size_t currentDirtyCount(const PushStateTracker &tracker)
        {
          return tracker.transaction_.current.dirtyStates.size();
        }
        static size_t nextDirtyCount(const PushStateTracker &tracker)
        {
          return tracker.transaction_.next.dirtyStates.size();
        }
        static size_t nextDeferredCount(const PushStateTracker &tracker)
        {
          return tracker.transaction_.next.deferred.size();
        }
        static void seedVisitPass(PushStateTracker &tracker, unsigned long value)
        {
          tracker.visitPass_ = value;
        }
        static size_t freeEntryCount(const PushStateTracker &tracker)
        {
          size_t count = 0;
          for (PushStateTracker::StateEntry *e = tracker.freeEntries_; e; e = e->next)
            ++count;
          return count;
        }
        static bool hasOrder(const PushStateTracker &tracker, StateBase *const *states, size_t count)
        {
          PushStateTracker::StateEntry *entry = tracker.statesHead_;
          for (size_t i = 0; i < count; ++i)
          {
            if (!entry || entry->state != states[i])
              return false;
            entry = entry->next;
          }
          return entry == 0;
        }
      };
    } // namespace testing
  } // namespace core
} // namespace loka

#endif
