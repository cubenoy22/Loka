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
          invalidateFn_(0),
          invalidateUserData_(0),
          statesHead_(0),
          statesTail_(0),
          freeEntries_(0),
          chunks_(0)
    {
    }

    PushStateTracker::PushStateTracker(const std::vector<StateBase *> &states)
        : phase_(TRACKER_IDLE),
          pendingDirty_(false),
          depth_(0),
          invalidateFn_(0),
          invalidateUserData_(0),
          statesHead_(0),
          statesTail_(0),
          freeEntries_(0),
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
      TransactionIntake &intake = transaction_.intake(phase_);
      intake.dirty = true;
      transaction_.anyDirty = true;
      pendingDirty_ = true;
      if (visiting_.count(state))
      {
        fprintf(stderr, "[Loka] Circular state dependency detected: StateBase %p\n", (void *)state);
        return;
      }
      visiting_.insert(state);
      DependencyMap::iterator it = dependents.find(state);
      if (it != dependents.end())
      {
        for (size_t i = 0; i < it->second.size(); ++i)
        {
          StateBase *dependent = it->second[i];
          markDirty(dependent);
        }
      }
      for (size_t i = 0; i < intake.dirtyStates.size(); ++i)
      {
        if (intake.dirtyStates[i] == state)
        {
          visiting_.erase(state);
          return;
        }
      }
      intake.dirtyStates.push_back(state);
      visiting_.erase(state);
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

      dependents.erase(state);
      for (DependencyMap::iterator it = dependents.begin(); it != dependents.end(); ++it)
      {
        StateList &list = it->second;
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
      visiting_.erase(state);
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
      allocateEntries(count);
    }

    bool PushStateTracker::end()
    {
      if (depth_ == 0)
        return true;
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
        StateList current = transaction_.current.dirtyStates;
        transaction_.current.dirtyStates.clear();
        for (size_t i = 0; i < current.size(); ++i)
        {
          StateBase *state = current[i];
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
               dependentIndex < it->second.size();
               ++dependentIndex)
          {
            markDirty(it->second[dependentIndex]);
          }
        }
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
        allocateEntries(1);
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

    bool PushStateTracker::peekDirty() const
    {
      return depth_ == 0 && pendingDirty_;
    }

    bool PushStateTracker::consumeDirty()
    {
      if (depth_ > 0)
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
      StateList &list = dependents[dependency];
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
