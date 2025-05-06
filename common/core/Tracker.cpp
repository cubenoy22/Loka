#include "Tracker.hpp"
#include "State.hpp"

StdTracker::StdTracker(const std::vector<StateBase *> &states)
    : phase_(TRACKER_IDLE), states_(states)
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

StdTracker::StdTracker() : phase_(TRACKER_IDLE) {}

void StdTracker::begin()
{
  for (auto *s : states_)
    s->currentTracker = this;
#ifdef TEST_BUILD
  printf("[begin] dependents.size()=%zu\n", dependents.size());
  for (auto &pair : dependents)
  {
    printf("[begin] dependents key=%p\n", (void *)pair.first);
  }
#endif
  dirtyStates.clear();
  deferred.clear();
  phase_ = TRACKER_PRECOMMIT;
}

void StdTracker::defer(void (*fn)(void *), void *userData)
{
  deferred.push_back(std::make_pair(fn, userData));
}

void StdTracker::markDirty(StateBase *state)
{
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
  for (auto &pair : dependents)
  {
    printf("[markDirty] dependents key=%p\n", (void *)pair.first);
  }
#endif
  auto it = dependents.find(state);
  if (it != dependents.end())
  {
#ifdef TEST_BUILD
    printf("[markDirty] dependents[%p] has %zu items\n", (void *)state, it->second.size());
#endif
    for (auto dependent : it->second)
    {
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

bool StdTracker::end()
{
#ifdef TEST_BUILD
  printf("[end] dependents.size()=%zu\n", dependents.size());
  for (auto &pair : dependents)
  {
    printf("[end] dependents key=%p\n", (void *)pair.first);
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
        auto it = dependents.find(s);
        if (it != dependents.end())
        {
          for (auto dependent : it->second)
          {
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
  phase_ = TRACKER_IDLE;
  for (auto *s : states_)
    s->currentTracker = nullptr;
  deferred.clear();
  return dirtyStates.empty();
}

void StdTracker::registerDependency(StateBase *dependent, StateBase *dependency)
{
#ifdef TEST_BUILD
  printf("[registerDependency] dependent=%p, dependency=%p\n", (void *)dependent, (void *)dependency);
#endif
  dependents[dependency].push_back(dependent);
#ifdef TEST_BUILD
  printf("[registerDependency] dependents.size()=%zu\n", dependents.size());
  for (auto &pair : dependents)
  {
    printf("[registerDependency] dependents key=%p\n", (void *)pair.first);
  }
#endif
}

TrackerPhase StdTracker::phase() const
{
  return phase_;
}

StdTracker::~StdTracker() {}
