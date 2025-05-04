#ifndef DECLARA_PROPERTY_HPP
#define DECLARA_PROPERTY_HPP

#include <string>
#include <vector>
#include "Transaction.hpp"
#include "State.hpp"

class PropBase
{
public:
  virtual bool recompute() = 0;
  virtual ~PropBase() {}
};

enum PropPriority
{
  PROP_PRIORITY_DEFER = -1,
  PROP_PRIORITY_NORMAL = 0,
  PROP_PRIORITY_HIGH = 100
};

// 型安全なbind/unbindを持つ抽象基底クラス
// PropBaseを継承し、T型のOnChangeFn/void* userDataで統一
// C++98対応

template <typename T>
class BindableProp : public PropBase
{
public:
  typedef void (*OnChangeFn)(T, void *);
  virtual void bind(OnChangeFn cb, void *userData, bool callImmediately = true, bool callOnce = false, int priority = PROP_PRIORITY_NORMAL) = 0;
  virtual void unbind(OnChangeFn cb, void *userData) = 0;
};

template <typename T, typename S>
class DerivedProp : public BindableProp<T>
{
public:
  typedef T (*EvalFn)(const S &);
  typedef void (*OnChangeFn)(T, void *);
  struct Handler
  {
    OnChangeFn fn;
    void *userData;
    bool callOnce;
    int priority;
    bool operator==(const Handler &other) const
    {
      return fn == other.fn && userData == other.userData;
    }
  };
  std::vector<Handler> handlers;

  DerivedProp(Tracker *tracker, State<S> *state, EvalFn eval)
      : tracker_(tracker), source(state), evalFn(eval)
  {
    cached = eval(source->get());
    if (tracker_ && source)
      tracker_->registerProp(this, source); // 依存元StateをTrackerへ登録
  }

  // 複数依存元対応のコンストラクタ
  DerivedProp(Tracker *tracker, const std::vector<State<S> *> &states, EvalFn eval)
      : tracker_(tracker), source(states.empty() ? 0 : states[0]), evalFn(eval)
  {
    if (tracker_ && !states.empty())
    {
      std::vector<StateBase *> deps;
      for (size_t i = 0; i < states.size(); ++i)
      {
        deps.push_back(states[i]);
      }
      tracker_->registerProp(this, deps);
    }
    if (source)
      cached = eval(source->get());
  }

  bool recompute()
  {
    T newVal;
    // dryRun中ならTrackerから仮値を取得して評価
    if (tracker_ && tracker_->getDryRunValue(source, newVal))
    {
      // 仮値で評価
      newVal = evalFn(newVal);
    }
    else
    {
      // 通常のget()で評価
      newVal = evalFn(source->get());
    }
    if (newVal != cached)
    {
      cached = newVal;
      for (size_t i = 0; i < handlers.size();)
      {
        handlers[i].fn(cached, handlers[i].userData);
        if (handlers[i].callOnce)
        {
          handlers.erase(handlers.begin() + i);
        }
        else
        {
          ++i;
        }
      }
      return true;
    }
    return false;
  }
  void bind(OnChangeFn cb, void *userData, bool callImmediately = true, bool callOnce = false, int priority = PROP_PRIORITY_NORMAL)
  {
    Handler h = {cb, userData, callOnce, priority};
    // priority降順で挿入
    std::vector<Handler>::iterator it = handlers.begin();
    for (; it != handlers.end(); ++it)
    {
      if (priority > it->priority)
        break;
    }
    handlers.insert(it, h);
    if (callImmediately && cb)
    {
      cb(cached, userData);
      if (callOnce)
        unbind(cb, userData);
    }
  }
  void unbind(OnChangeFn cb, void *userData)
  {
    Handler target{cb, userData, false, 0};
    for (size_t i = 0; i < handlers.size(); ++i)
    {
      if (handlers[i] == target)
      {
        handlers.erase(handlers.begin() + i);
        break;
      }
    }
  }
  T get() const { return cached; }

private:
  Tracker *tracker_;
  State<S> *source;
  EvalFn evalFn;
  T cached;
};

template <typename T>
class StaticProp : public BindableProp<T>
{
public:
  typedef void (*OnChangeFn)(T, void *);
  StaticProp(const T &v) : value(v) {}
  bool recompute() { return false; }
  T get() const { return value; }
  // 値が変わらないのでbind/unbindは即時コールのみ
  void bind(OnChangeFn cb, void *userData, bool callImmediately = true, bool callOnce = false, int priority = PROP_PRIORITY_NORMAL)
  {
    if (callImmediately && cb)
    {
      cb(value, userData);
      // callOnce==trueでも即時コールのみ
    }
  }
  void unbind(OnChangeFn cb, void *userData)
  {
    // 何もしない
  }

private:
  T value;
};

extern StaticProp<bool> TRUE;
extern StaticProp<bool> FALSE;

#endif // DECLARA_PROPERTY_HPP
