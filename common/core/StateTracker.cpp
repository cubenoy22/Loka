#include "StateTracker.hpp"
#include "State.hpp"

namespace declara
{
  namespace core
  {

    PushStateTracker::PushStateTracker()
        : phase_(TRACKER_IDLE), dirtyFlag_(false), invalidateFn_(0), invalidateUserData_(0),
          statesHead_(0), statesTail_(0), freeEntries_(0) {}

    PushStateTracker::PushStateTracker(const std::vector<StateBase *> &states)
        : phase_(TRACKER_IDLE), dirtyFlag_(false), invalidateFn_(0), invalidateUserData_(0),
          statesHead_(0), statesTail_(0), freeEntries_(0)
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
      for (StateEntry *e = statesHead_; e; e = e->next)
        e->state->currentTracker = this;
#ifdef TEST_BUILD
      printf("[begin] dependents.size()=%zu\n", dependents.size());
  for (DependencyMap::iterator it = dependents.begin(); it != dependents.end(); ++it)
      {
        printf("[begin] dependents key=%p\n", (void *)it->first);
      }
#endif
      dirtyStates.clear();
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
#ifdef TEST_BUILD
      printf("[markDirty] state=%p\n", (void *)state);
#endif
      if (visiting_.count(state))
      {
        fprintf(stderr, "[Declara] 循環依存検出: StateBase %p\n", (void *)state);
        return;
      }
      visiting_.insert(state);
#ifdef TEST_BUILD
      printf("[markDirty] dependents.size()=%zu\n", dependents.size());
  for (DependencyMap::iterator itDep = dependents.begin(); itDep != dependents.end(); ++itDep)
      {
        printf("[markDirty] dependents key=%p\n", (void *)itDep->first);
      }
#endif
  DependencyMap::iterator it = dependents.find(state);
      if (it != dependents.end())
      {
#ifdef TEST_BUILD
        printf("[markDirty] dependents[%p] has %zu items\n", (void *)state, it->second.size());
#endif
        for (size_t i = 0; i < it->second.size(); ++i)
        {
          StateBase *dependent = it->second[i];
#ifdef TEST_BUILD
          printf("[markDirty] state=%p -> dependent=%p\n", (void *)state, (void *)dependent);
#endif
          markDirty(dependent);
        }
      }
      else
      {
#ifdef TEST_BUILD
        printf("[markDirty] dependents[%p] not found\n", (void *)state);
#endif
      }
      for (size_t i = 0; i < dirtyStates.size(); ++i)
      {
        if (dirtyStates[i] == state)
        {
#ifdef TEST_BUILD
          printf("[markDirty] state=%p is already dirty, skipping\n", (void *)state);
#endif
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

    bool PushStateTracker::end()
    {
#ifdef TEST_BUILD
      printf("[end] dependents.size()=%zu\n", dependents.size());
  for (DependencyMap::iterator it = dependents.begin(); it != dependents.end(); ++it)
      {
        printf("[end] dependents key=%p\n", (void *)it->first);
      }
#endif
      size_t maxIter = 1000;
      while (!dirtyStates.empty() && maxIter--)
      {
#ifdef TEST_BUILD
        printf("[end] dirtyStates.size()=%zu\n", dirtyStates.size());
        for (size_t i = 0; i < dirtyStates.size(); ++i)
        {
          printf("[end] dirtyStates[%zu]=%p\n", i, (void *)dirtyStates[i]);
        }
#endif
        std::vector<StateBase *> current = dirtyStates;
        dirtyStates.clear();
        for (size_t i = 0; i < current.size(); ++i)
        {
          StateBase *s = current[i];
          bool changed = s->recompute();
          if (changed)
          {
            // 依存先（dependents）をdirtyにする
            DependencyMap::iterator it = dependents.find(s);
            if (it != dependents.end())
            {
              for (size_t j = 0; j < it->second.size(); ++j)
              {
                StateBase *dependent = it->second[j];
#ifdef TEST_BUILD
                printf("[end] propagate dirty: %p -> %p\n", (void *)s, (void *)dependent);
#endif
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
        entry = new StateEntry();
      }
      entry->state = state;
      entry->next = 0;
      return entry;
    }

    void PushStateTracker::releaseEntries()
    {
      StateEntry *entry = statesHead_;
      while (entry)
      {
        StateEntry *next = entry->next;
        delete entry;
        entry = next;
      }
      statesHead_ = 0;
      statesTail_ = 0;

      StateEntry *freeEntry = freeEntries_;
      while (freeEntry)
      {
        StateEntry *next = freeEntry->next;
        delete freeEntry;
        freeEntry = next;
      }
      freeEntries_ = 0;
    }

    bool PushStateTracker::consumeDirty()
    {
      bool dirty = dirtyFlag_;
      dirtyFlag_ = false;
      return dirty;
    }

    void PushStateTracker::registerDependency(StateBase *dependent, StateBase *dependency)
    {
#ifdef TEST_BUILD
      printf("[registerDependency] dependent=%p, dependency=%p\n", (void *)dependent, (void *)dependency);
#endif
      dependents[dependency].push_back(dependent);
#ifdef TEST_BUILD
      printf("[registerDependency] dependents.size()=%zu\n", dependents.size());
  for (DependencyMap::iterator it = dependents.begin(); it != dependents.end(); ++it)
      {
        printf("[registerDependency] dependents key=%p\n", (void *)it->first);
      }
#endif
    }

    TrackerPhase PushStateTracker::phase() const
    {
      return phase_;
    }


  } // namespace core
} // namespace declara
