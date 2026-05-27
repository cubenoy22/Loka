#include "loka/core/StateTracker.hpp"
#include "loka/core/State.hpp"

namespace loka
{
  namespace core
  {

    PushStateTracker::PushStateTracker()
        : phase_(TRACKER_IDLE),
          dirtyFlag_(false),
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
          dirtyFlag_(false),
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
      dirtyStates.clear();
      committedDirtyStates_.clear();
      deferred.clear();
      dirtyFlag_ = false;
      phase_ = TRACKER_PRECOMMIT;
    }

    void PushStateTracker::defer(void (*fn)(void *), void *userData)
    {
      deferred.push_back(std::make_pair(fn, userData));
    }

    void PushStateTracker::markDirty(StateBase *state)
    {
      dirtyFlag_ = true;
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
      for (size_t i = 0; i < dirtyStates.size(); ++i)
      {
        if (dirtyStates[i] == state)
        {
          visiting_.erase(state);
          return;
        }
      }
      dirtyStates.push_back(state);
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
      for (size_t i = 0; i < committedDirtyStates_.size();)
      {
        if (committedDirtyStates_[i] == state)
        {
          committedDirtyStates_.erase(committedDirtyStates_.begin() + i);
        }
        else
        {
          ++i;
        }
      }

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
      size_t maxIter = 1000;
      while (!dirtyStates.empty() && maxIter--)
      {
        std::vector<StateBase *> current = dirtyStates;
        dirtyStates.clear();
        for (size_t i = 0; i < current.size(); ++i)
        {
          StateBase *s = current[i];
          bool alreadyCommitted = false;
          for (size_t committedIndex = 0; committedIndex < committedDirtyStates_.size(); ++committedIndex)
          {
            if (committedDirtyStates_[committedIndex] == s)
            {
              alreadyCommitted = true;
              break;
            }
          }
          if (!alreadyCommitted)
          {
            committedDirtyStates_.push_back(s);
          }
          bool changed = s->recompute();
          if (changed)
          {
            // Mark dependent states dirty after a derived value changes.
            DependencyMap::iterator it = dependents.find(s);
            if (it != dependents.end())
            {
              for (size_t j = 0; j < it->second.size(); ++j)
              {
                StateBase *dependent = it->second[j];
                markDirty(dependent);
              }
            }
          }
        }
      }
      phase_ = TRACKER_COMMIT;
      for (size_t i = 0; i < deferred.size(); ++i)
      {
        deferred[i].first(deferred[i].second);
      }
      if (invalidateFn_ && dirtyFlag_)
      {
        invalidateFn_(invalidateUserData_);
      }
      phase_ = TRACKER_IDLE;
      for (StateEntry *e = statesHead_; e; e = e->next)
        e->state->currentTracker = 0;
      deferred.clear();
      return dirtyStates.empty();
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

    bool PushStateTracker::consumeDirty()
    {
      if (depth_ > 0)
      {
        return false;
      }
      bool dirty = dirtyFlag_;
      dirtyFlag_ = false;
      return dirty;
    }

    void PushStateTracker::registerDependency(StateBase *dependent, StateBase *dependency)
    {
      dependents[dependency].push_back(dependent);
    }

    TrackerPhase PushStateTracker::phase() const
    {
      return phase_;
    }

  } // namespace core
} // namespace loka
