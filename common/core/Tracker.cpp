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

void StdTracker::set(StateBase *s, const void *v)
{
  // 型安全なsetは利用側でキャストして呼ぶ
  markDirty(s);
}

void StdTracker::defer(void (*fn)(void *), void *userData)
{
  deferred.push_back(std::make_pair(fn, userData));
}

void StdTracker::markDirty(StateBase *state)
{
  for (size_t i = 0; i < dirtyStates.size(); ++i)
  {
    if (dirtyStates[i] == state)
      return;
  }
  dirtyStates.push_back(state);
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
      s->recompute();
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
