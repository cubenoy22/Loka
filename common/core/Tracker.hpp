#ifndef DECLARA_TRACKER_HPP
#define DECLARA_TRACKER_HPP

#include <vector>
#include <functional>
#include <map>
#include <memory>
#include <string>

// 必要な前方宣言
class StateBase;

// TrackerのPhase定義
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
  StdTracker(const std::vector<StateBase *> &states);
  StdTracker();
  void begin() override;
  void set(StateBase *s, const void *v) override;
  void defer(void (*fn)(void *), void *userData) override;
  void markDirty(StateBase *state) override;
  bool end() override;
  void registerDependency(StateBase *dependent, StateBase *dependency) override;
  TrackerPhase phase() const override;
  ~StdTracker();

private:
  std::vector<StateBase *> dirtyStates;
  std::vector<std::pair<void (*)(void *), void *>> deferred;
  std::map<StateBase *, std::vector<StateBase *>> dependents;
  TrackerPhase phase_;
};

#endif // DECLARA_TRACKER_HPP
