#ifndef LOKA_STATETRACKER_HPP
#define LOKA_STATETRACKER_HPP

#include "core/diag/LifecycleAudit.hpp"
#include <vector>
#include <functional>
#include <map>
#include <string>
#include <set>

namespace loka
{
  namespace core
  {

    class StateBase;

    // StateTracker transaction phase.
    enum TrackerPhase
    {
      TRACKER_IDLE = 0,
      TRACKER_PRECOMMIT = 1,
      TRACKER_COMMIT = 2
    };

    class PushStateTracker; // forward declaration

    class StateTracker
    {
    public:
      virtual void begin() = 0;
      virtual void defer(void (*fn)(void *), void *userData) = 0;
      virtual void markDirty(StateBase *state) = 0;
      virtual bool end() = 0;
      virtual void registerDependency(StateBase *dependent, StateBase *dependency) = 0;
      virtual TrackerPhase phase() const = 0;
      virtual PushStateTracker *asPushTracker()
      {
        return 0;
      }
      virtual ~StateTracker() {}
    };

    class PushStateTracker : public StateTracker LOKA_AUDITED_AS(PushStateTracker)
    {
    public:
      typedef void (*InvalidateFn)(void *);
      typedef std::vector<StateBase *> StateList;
      PushStateTracker(const std::vector<StateBase *> &states);
      PushStateTracker();
      void begin();
      void defer(void (*fn)(void *), void *userData);
      void markDirty(StateBase *state);
      void addState(StateBase *state);
      void addStateUnchecked(StateBase *state);
      void removeState(StateBase *state);
      void reserveStates(size_t count);
      bool end();
      /** Returns whether the current (or just-ended) transaction marked any state dirty. */
      bool transactionDirty() const
      {
        return transaction_.anyDirty;
      }
      /** Returns whether committed dirt is waiting to be acknowledged. */
      bool peekDirty() const;
      /** Returns and acknowledges committed dirt. */
      bool consumeDirty();
      const StateList &committedDirtyStates() const
      {
        return transaction_.committedDirtyStates;
      }
      void setInvalidateCallback(InvalidateFn fn, void *userData)
      {
        invalidateFn_ = fn;
        invalidateUserData_ = userData;
      }
      /**
       * Registers a dependency edge used for derived-state propagation.
       * Usually built from DerivedState dependencies.
       */
      void registerDependency(StateBase *dependent, StateBase *dependency);
      TrackerPhase phase() const;
      PushStateTracker *asPushTracker()
      {
        return this;
      }
      const PushStateTracker *asPushTracker() const
      {
        return this;
      }
      ~PushStateTracker();

    private:
      struct StateEntry
      {
        StateEntry()
            : state(0),
              next(0)
        {
        }
        StateBase *state;
        StateEntry *next;
      };
      struct StateEntryChunk
      {
        StateEntryChunk()
            : entries(0),
              count(0),
              next(0)
        {
        }
        StateEntry *entries;
        size_t count;
        StateEntryChunk *next;
      };

      // Typedefs to avoid nested template closers in C++98
      typedef std::pair<void (*)(void *), void *> DeferredEntry;
      typedef std::vector<DeferredEntry> DeferredList;
      typedef std::map<StateBase *, StateList> DependencyMap;

      /** Mutable intake owned by one side of a tracker transaction. */
      struct TransactionIntake
      {
        TransactionIntake()
            : dirtyStates(),
              deferred(),
              dirty(false)
        {
        }

        void clear();
        bool hasWork() const;
        void removeState(StateBase *state);
        void swap(TransactionIntake &other);

        StateList dirtyStates;
        DeferredList deferred;
        bool dirty;
      };

      /** Owns the current transaction, its snapshot, and next-commit intake. */
      struct TrackerTransaction
      {
        TrackerTransaction()
            : current(),
              next(),
              committedDirtyStates(),
              anyDirty(false)
        {
        }

        void begin();
        TransactionIntake &intake(TrackerPhase phase);
        void advance();
        void removeState(StateBase *state);

        TransactionIntake current;
        TransactionIntake next;
        StateList committedDirtyStates;
        bool anyDirty;
      };

      TrackerTransaction transaction_;
      /// dependents: dependency graph from a source state to dependent states.
      DependencyMap dependents;
      /// phase_: current tracker transaction phase.
      TrackerPhase phase_;
      /// pendingDirty_: committed dirt waiting for owner acknowledgment.
      bool pendingDirty_;
      /// depth_: nested begin/end depth counter.
      unsigned int depth_;
      /// invalidate callback (optional)
      InvalidateFn invalidateFn_;
      void *invalidateUserData_;
      /// visiting_: temporary set used to detect cycles during recursive propagation.
      std::set<StateBase *> visiting_;
      /// states: linked list (head/tail for O(1) append)
      StateEntry *statesHead_;
      StateEntry *statesTail_;
      StateEntry *freeEntries_;
      StateEntryChunk *chunks_;

      StateEntry *allocateEntry(StateBase *state);
      void allocateEntries(size_t count);
      void releaseEntries();
      bool settleCurrentTransaction(size_t &iterationsRemaining);
    };

  } // namespace core
} // namespace loka

#endif // LOKA_STATETRACKER_HPP
