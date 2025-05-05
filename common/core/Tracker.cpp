#include "Tracker.hpp"
#include "State.hpp"

StdTracker::StdTracker(const std::vector<StateBase *> &states)
    : phase_(TRACKER_IDLE)
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
  // 循環依存検出: すでに伝播中なら警告を出してreturn
  if (visiting_.count(state))
  {
    fprintf(stderr, "[Declara] 循環依存検出: StateBase %p\n", (void *)state);
    return;
  }
  visiting_.insert(state);
  // すでにdirtyなら何もしない
  for (size_t i = 0; i < dirtyStates.size(); ++i)
  {
    if (dirtyStates[i] == state)
    {
      visiting_.erase(state);
      return;
    }
  }
  dirtyStates.push_back(state);
  // 依存先も再帰的にdirty化
  auto it = dependents.find(state);
  if (it != dependents.end())
  {
    for (auto dependent : it->second)
    {
      markDirty(dependent);
    }
  }
  visiting_.erase(state);
}

bool StdTracker::end()
{
  size_t maxIter = 1000;
  while (!dirtyStates.empty() && maxIter--)
  {
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
  deferred.clear();
  return dirtyStates.empty();
}

void StdTracker::registerDependency(StateBase *dependent, StateBase *dependency)
{
  dependents[dependency].push_back(dependent);
}

TrackerPhase StdTracker::phase() const
{
  return phase_;
}

StdTracker::~StdTracker() {}
