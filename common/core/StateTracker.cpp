#include "StateTracker.hpp"
#include "State.hpp"

namespace declara
{
  namespace core
  {

    PushStateTracker::PushStateTracker(const std::vector<StateBase *> &states)
        : phase_(TRACKER_IDLE), dirtyFlag_(false), invalidateFn_(0), invalidateUserData_(0), states_(states)
    {
      for (size_t i = 0; i < states.size(); ++i)
      {
        std::vector<StateBase *> deps = states[i]->getDependencyStates();
        if (!deps.empty())
        {
          for (size_t j = 0; j < deps.size(); ++j)
          {
            registerDependency(states[i], deps[j]);
          }
        }
      }
    }

    PushStateTracker::PushStateTracker()
        : phase_(TRACKER_IDLE), dirtyFlag_(false), invalidateFn_(0), invalidateUserData_(0) {}

    void PushStateTracker::begin()
    {
      for (size_t i = 0; i < states_.size(); ++i)
        states_[i]->currentTracker = this;
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
      for (size_t i = 0; i < states_.size(); ++i)
      {
        if (states_[i] == state)
        {
          return;
        }
      }
      states_.push_back(state);
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
      states_.push_back(state);
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

    void PushStateTracker::reserveStates(size_t count)
    {
      states_.reserve(states_.size() + count);
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
      for (size_t i = 0; i < states_.size(); ++i)
        states_[i]->currentTracker = 0;
      deferred.clear();
      return dirtyStates.empty();
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

    PushStateTracker::~PushStateTracker() {}

  } // namespace core
} // namespace declara
