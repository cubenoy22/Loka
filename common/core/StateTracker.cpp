#include "core/StateTracker.hpp"
#include "core/State.hpp"
#include <cstdio>

namespace loka
{
  namespace core
  {

    PushStateTracker::PushStateTracker()
        : phase_(TRACKER_IDLE),
          pendingDirty_(false),
          depth_(0),
          reentrantDepth_(0),
          invalidateFn_(0),
          invalidateUserData_(0),
          invalidateTarget_(0),
          visitPass_(0),
          initialEntry_(),
          statesHead_(0),
          statesTail_(0),
          freeEntries_(&initialEntry_),
          chunks_(0)
    {
    }

    PushStateTracker::PushStateTracker(const std::vector<StateBase *> &states)
        : phase_(TRACKER_IDLE),
          pendingDirty_(false),
          depth_(0),
          reentrantDepth_(0),
          invalidateFn_(0),
          invalidateUserData_(0),
          invalidateTarget_(0),
          visitPass_(0),
          initialEntry_(),
          statesHead_(0),
          statesTail_(0),
          freeEntries_(&initialEntry_),
          chunks_(0)
    {
      for (size_t i = 0; i < states.size(); ++i)
      {
        addState(states[i]);
      }
    }

    PushStateTracker::~PushStateTracker()
    {
      releaseEntries();
    }

    void PushStateTracker::begin()
    {
      if (depth_ > 0)
      {
        ++depth_;
        return;
      }
      if (phase_ != TRACKER_IDLE)
      {
        ++reentrantDepth_;
        return;
      }
      ++depth_;
      for (StateEntry *e = statesHead_; e; e = e->next)
        e->state->currentTracker = this;
      transaction_.begin();
      phase_ = TRACKER_PRECOMMIT;
    }

    void PushStateTracker::defer(void (*fn)(void *), void *userData)
    {
      transaction_.intake(phase_).deferred.push_back(std::make_pair(fn, userData));
    }

    void PushStateTracker::markDirty(StateBase *state)
    {
      if (!state)
      {
        return;
      }
      // Every completed walk clears its active stamps. On unsigned wrap,
      // skip the idle sentinel; no graph-wide reset is needed.
      if (++this->visitPass_ == 0)
        ++this->visitPass_;
      this->propagateDirty(state, this->visitPass_);
    }

    void PushStateTracker::propagateDirty(StateBase *state, unsigned long pass)
    {
      TransactionIntake &intake = transaction_.intake(phase_);
      intake.dirty = true;
      transaction_.anyDirty = true;
      pendingDirty_ = true;
      DependencyMap::iterator it = dependents.find(state);
      if (it != dependents.end())
      {
        Dependents &row = it->second;
        if (row.visitPass == pass)
        {
          fprintf(stderr, "[Loka] Circular state dependency detected: StateBase %p\n", (void *)state);
          return;
        }
        row.visitPass = pass;
        for (size_t i = 0; i < row.states.size(); ++i)
        {
          this->propagateDirty(row.states[i], pass);
        }
        row.visitPass = 0;
      }
      // A walk stamp describes the active path, not intake membership across
      // separate writes. Keep the duplicate scan for both rows and leaves.
      for (size_t i = 0; i < intake.dirtyStates.size(); ++i)
      {
        if (intake.dirtyStates[i] == state)
        {
          return;
        }
      }
      intake.dirtyStates.push_back(state);
    }

    void PushStateTracker::addState(StateBase *state)
    {
      if (!state)
      {
        return;
      }
      // Check for duplicates by traversing the linked list
      for (StateEntry *e = statesHead_; e; e = e->next)
      {
        if (e->state == state)
        {
          return;
        }
      }
      // Append to linked list
      StateEntry *entry = allocateEntry(state);
      if (statesTail_)
      {
        statesTail_->next = entry;
      }
      else
      {
        statesHead_ = entry;
      }
      statesTail_ = entry;

      std::vector<StateBase *> deps = state->getDependencyStates();
      if (!deps.empty())
      {
        for (size_t j = 0; j < deps.size(); ++j)
        {
          registerDependency(state, deps[j]);
        }
      }
      if (phase_ != TRACKER_IDLE)
      {
        state->currentTracker = this;
      }
    }

    void PushStateTracker::addStateUnchecked(StateBase *state)
    {
      if (!state)
      {
        return;
      }
      // Skip duplicate check - caller guarantees uniqueness
      // Append to linked list
      StateEntry *entry = allocateEntry(state);
      if (statesTail_)
      {
        statesTail_->next = entry;
      }
      else
      {
        statesHead_ = entry;
      }
      statesTail_ = entry;

      std::vector<StateBase *> deps = state->getDependencyStates();
      if (!deps.empty())
      {
        for (size_t j = 0; j < deps.size(); ++j)
        {
          registerDependency(state, deps[j]);
        }
      }
      if (phase_ != TRACKER_IDLE)
      {
        state->currentTracker = this;
      }
    }

    void PushStateTracker::removeState(StateBase *state)
    {
      if (!state)
      {
        return;
      }

      StateEntry *prev = 0;
      StateEntry *entry = statesHead_;
      while (entry)
      {
        if (entry->state == state)
        {
          StateEntry *next = entry->next;
          if (prev)
          {
            prev->next = next;
          }
          else
          {
            statesHead_ = next;
          }
          if (statesTail_ == entry)
          {
            statesTail_ = prev;
          }
          entry->state = 0;
          entry->next = freeEntries_;
          freeEntries_ = entry;
          break;
        }
        prev = entry;
        entry = entry->next;
      }

      transaction_.removeState(state);
      // Preserve the active loop index when a recompute removes a later state.
      for (size_t i = 0; i < this->scratch_.size(); ++i)
      {
        if (this->scratch_[i] == state)
          this->scratch_[i] = 0;
      }

      dependents.erase(state);
      for (DependencyMap::iterator it = dependents.begin(); it != dependents.end(); ++it)
      {
        StateList &list = it->second.states;
        for (size_t i = 0; i < list.size();)
        {
          if (list[i] == state)
          {
            list.erase(list.begin() + i);
          }
          else
          {
            ++i;
          }
        }
      }
      if (state->currentTracker == this)
      {
        state->currentTracker = 0;
      }
    }

    void PushStateTracker::reserveStates(size_t count)
    {
      if (count == 0)
      {
        return;
      }
      transaction_.current.dirtyStates.reserve(count);
      transaction_.next.dirtyStates.reserve(count);
      transaction_.committedDirtyStates.reserve(count);
      this->scratch_.reserve(count);
      allocateEntries(count);
    }

    bool PushStateTracker::end()
    {
      if (depth_ == 0)
      {
        if (reentrantDepth_ > 0)
        {
          --reentrantDepth_;
        }
        return true;
      }
      --depth_;
      if (depth_ > 0)
        return true;
      size_t stateIterationsRemaining = 1000;
      size_t commitIterationsRemaining = 1000;
      bool settled = true;
      while (commitIterationsRemaining > 0)
      {
        --commitIterationsRemaining;
        phase_ = TRACKER_PRECOMMIT;
        if (!settleCurrentTransaction(stateIterationsRemaining))
        {
          settled = false;
          fprintf(stderr, "[Loka] StateTracker transaction did not settle before the iteration limit.\n");
          break;
        }

        phase_ = TRACKER_COMMIT;
        for (size_t i = 0; i < transaction_.current.deferred.size(); ++i)
        {
          transaction_.current.deferred[i].first(
              transaction_.current.deferred[i].second);
        }
        transaction_.current.deferred.clear();
        if (invalidateFn_ && transaction_.current.dirty)
        {
          invalidateFn_(invalidateUserData_);
        }
        if (!transaction_.next.hasWork())
        {
          break;
        }
        if (commitIterationsRemaining == 0)
        {
          break;
        }
        transaction_.advance();
      }
      if (settled && transaction_.next.hasWork())
      {
        settled = false;
        fprintf(stderr, "[Loka] StateTracker commit chain did not settle before the iteration limit.\n");
      }
      phase_ = TRACKER_IDLE;
      for (StateEntry *e = statesHead_; e; e = e->next)
        e->state->currentTracker = 0;
      return settled;
    }

    bool PushStateTracker::settleCurrentTransaction(size_t &iterationsRemaining)
    {
      while (!transaction_.current.dirtyStates.empty() && iterationsRemaining > 0)
      {
        --iterationsRemaining;
        this->scratch_.swap(transaction_.current.dirtyStates);
        for (size_t i = 0; i < this->scratch_.size(); ++i)
        {
          StateBase *state = this->scratch_[i];
          if (!state)
            continue;
          bool alreadyCommitted = false;
          for (size_t committedIndex = 0;
               committedIndex < transaction_.committedDirtyStates.size();
               ++committedIndex)
          {
            if (transaction_.committedDirtyStates[committedIndex] == state)
            {
              alreadyCommitted = true;
              break;
            }
          }
          if (!alreadyCommitted)
          {
            transaction_.committedDirtyStates.push_back(state);
          }
          if (!state->recompute())
          {
            continue;
          }
          DependencyMap::iterator it = dependents.find(state);
          if (it == dependents.end())
          {
            continue;
          }
          for (size_t dependentIndex = 0;
               dependentIndex < it->second.states.size();
               ++dependentIndex)
          {
            markDirty(it->second.states[dependentIndex]);
          }
        }
        this->scratch_.clear();
      }
      return transaction_.current.dirtyStates.empty();
    }

    void PushStateTracker::TransactionIntake::clear()
    {
      dirtyStates.clear();
      deferred.clear();
      dirty = false;
    }

    bool PushStateTracker::TransactionIntake::hasWork() const
    {
      return dirty || !dirtyStates.empty() || !deferred.empty();
    }

    void PushStateTracker::TransactionIntake::removeState(StateBase *state)
    {
      for (size_t i = 0; i < dirtyStates.size();)
      {
        if (dirtyStates[i] == state)
        {
          dirtyStates.erase(dirtyStates.begin() + i);
        }
        else
        {
          ++i;
        }
      }
    }

    void PushStateTracker::TransactionIntake::swap(TransactionIntake &other)
    {
      dirtyStates.swap(other.dirtyStates);
      deferred.swap(other.deferred);
      const bool otherDirty = other.dirty;
      other.dirty = dirty;
      dirty = otherDirty;
    }

    void PushStateTracker::TrackerTransaction::begin()
    {
      current.clear();
      next.clear();
      committedDirtyStates.clear();
      anyDirty = false;
    }

    PushStateTracker::TransactionIntake &
    PushStateTracker::TrackerTransaction::intake(TrackerPhase phase)
    {
      return phase == TRACKER_COMMIT ? next : current;
    }

    void PushStateTracker::TrackerTransaction::advance()
    {
      current.clear();
      current.swap(next);
      next.clear();
      committedDirtyStates.clear();
    }

    void PushStateTracker::TrackerTransaction::removeState(StateBase *state)
    {
      current.removeState(state);
      next.removeState(state);
      for (size_t i = 0; i < committedDirtyStates.size();)
      {
        if (committedDirtyStates[i] == state)
        {
          committedDirtyStates.erase(committedDirtyStates.begin() + i);
        }
        else
        {
          ++i;
        }
      }
    }

    PushStateTracker::StateEntry *PushStateTracker::allocateEntry(StateBase *state)
    {
      StateEntry *entry = 0;
      if (freeEntries_)
      {
        entry = freeEntries_;
        freeEntries_ = freeEntries_->next;
      }
      else
      {
        // The first row is inline; amortize further unreserved growth. The
        // chunk chain owns only heap rows, and removals recycle either kind.
        enum { kEntryChunkCapacity = 16 };
        allocateEntries(kEntryChunkCapacity);
        entry = freeEntries_;
        freeEntries_ = freeEntries_->next;
      }
      entry->state = state;
      entry->next = 0;
      return entry;
    }

    void PushStateTracker::allocateEntries(size_t count)
    {
      StateEntry *entries = new StateEntry[count];
      StateEntryChunk *chunk = new StateEntryChunk();
      chunk->entries = entries;
      chunk->count = count;
      chunk->next = chunks_;
      chunks_ = chunk;
      for (size_t i = 0; i < count; ++i)
      {
        StateEntry *entry = &entries[i];
        entry->next = freeEntries_;
        freeEntries_ = entry;
      }
    }

    void PushStateTracker::releaseEntries()
    {
      statesHead_ = 0;
      statesTail_ = 0;
      freeEntries_ = 0;
      while (chunks_)
      {
        StateEntryChunk *next = chunks_->next;
        delete[] chunks_->entries;
        delete chunks_;
        chunks_ = next;
      }
    }

    // A transaction is closed only when no level is open and no settlement is
    // running: a level joined during settlement (begin() while phase_ is not
    // idle) sees depth_ == 0 yet must not read or clear the owner's pending
    // dirt; the outer end() owns it until the phase returns to idle.
    bool PushStateTracker::peekDirty() const
    {
      return depth_ == 0 && phase_ == TRACKER_IDLE && pendingDirty_;
    }

    bool PushStateTracker::consumeDirty()
    {
      if (depth_ > 0 || phase_ != TRACKER_IDLE)
      {
        return false;
      }
      bool dirty = pendingDirty_;
      pendingDirty_ = false;
      return dirty;
    }

    void PushStateTracker::registerDependency(StateBase *dependent, StateBase *dependency)
    {
      if (!dependent || !dependency)
      {
        return;
      }
      StateList &list = dependents[dependency].states;
      for (size_t i = 0; i < list.size(); ++i)
      {
        if (list[i] == dependent)
        {
          return;
        }
      }
      list.push_back(dependent);
    }

    TrackerPhase PushStateTracker::phase() const
    {
      return phase_;
    }

  } // namespace core
} // namespace loka
