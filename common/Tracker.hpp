#ifndef DECLARA_TRACKER_HPP
#define DECLARA_TRACKER_HPP

#include <vector>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include "ValueHolder.hpp"

enum TrackerPhase
{
  TRACKER_IDLE = 0,
  TRACKER_PRECOMMIT = 1,
  TRACKER_COMMIT = 2
};

class Tracker
{
public:
  virtual void begin() = 0;
  virtual void set(StateBase *s, const void *v) = 0; // 型安全なsetのみ
  virtual void defer(void (*fn)(void *), void *userData) = 0;
  virtual void markDirty(StateBase *state) = 0;
  virtual bool end() = 0;
  virtual void registerDependency(StateBase *dependent, StateBase *dependency) = 0;
  virtual TrackerPhase phase() const = 0;
  virtual ~Tracker() {}
};

class StdTracker : public Tracker
{
public:
  StdTracker(const std::vector<StateBase *> &states, const std::vector<StateBase *> &derivedStates)
  {
    for (size_t i = 0; i < derivedStates.size(); ++i)
    {
      std::vector<StateBase *> deps = derivedStates[i]->getDependencyStates();
      for (size_t j = 0; j < deps.size(); ++j)
      {
        registerDependency(derivedStates[i], deps[j]);
      }
    }
  }
  StdTracker() {}
  void begin() override
  {
    dirtyStates.clear();
    deferred.clear();
    phase_ = TRACKER_PRECOMMIT;
  }
  void set(StateBase *s, const void *v) override
  {
    // 型安全なsetは利用側でキャストして呼ぶ
    markDirty(s);
  }
  void defer(void (*fn)(void *), void *userData) override
  {
    deferred.push_back(std::make_pair(fn, userData));
  }
  void markDirty(StateBase *state) override
  {
    for (size_t i = 0; i < dirtyStates.size(); ++i)
    {
      if (dirtyStates[i] == state)
        return;
    }
    dirtyStates.push_back(state);
  }
  bool end() override
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
  void registerDependency(StateBase *dependent, StateBase *dependency) override
  {
    dependents[dependency].push_back(dependent);
  }
  TrackerPhase phase() const override { return phase_; }
  ~StdTracker() {}

private:
  std::vector<StateBase *> dirtyStates;
  std::vector<std::pair<void (*)(void *), void *>> deferred;
  std::map<StateBase *, std::vector<StateBase *>> dependents;
  TrackerPhase phase_ = TRACKER_IDLE;
};

#endif // DECLARA_TRACKER_HPP
