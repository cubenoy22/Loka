#ifndef DECLARA_BASETYPES_HPP
#define DECLARA_BASETYPES_HPP

#include <vector>
#include "ValueHolder.hpp"

// 前方宣言
class Tracker;

// PropBaseの基本定義
class PropBase
{
public:
  virtual bool recompute() = 0;
  virtual ~PropBase() {}
};

// StateBaseの基本定義
class StateBase : public PropBase
{
public:
  virtual ~StateBase() {}
  // 型消去: ValueHolderBase経由で値をセット
  virtual void setValue(const ValueHolderBase &) = 0;
  virtual void bindTracker(Tracker *tracker) = 0;
  // PropBaseのrecomputeをオーバーライド（Stateでは何もしない実装）
  virtual bool recompute() { return false; }
};

// BindablePropBaseの基本定義
class BindablePropBase : public PropBase
{
public:
  virtual std::vector<StateBase *> getDependencyStates() const = 0;
  virtual ~BindablePropBase() {}
};

// TrackerのPhase定義
enum TrackerPhase
{
  TRACKER_IDLE = 0,
  TRACKER_PRECOMMIT = 1,
  TRACKER_COMMIT = 2
};

// PropPriorityの定義
enum PropPriority
{
  PROP_PRIORITY_DEFER = -1,
  PROP_PRIORITY_NORMAL = 0,
  PROP_PRIORITY_HIGH = 100
};

#endif // DECLARA_BASETYPES_HPP
