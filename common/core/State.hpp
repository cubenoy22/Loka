#ifndef DECLARA_STATE_HPP
#define DECLARA_STATE_HPP

#include <vector>
#include <functional>
#include <memory>
#include "StateTracker.hpp"

// State系のbind/unbind等で使う優先度enum
enum StatePriority
{
  STATE_PRIORITY_DEFER = -1,
  STATE_PRIORITY_NORMAL = 0,
  STATE_PRIORITY_HIGH = 100
};

class StateTracker;

// StateBase: 依存管理・バインドAPIを統合
class StateBase
{
public:
  StateTracker *currentTracker = nullptr;
  virtual ~StateBase() {}
  // 依存State列挙（循環依存検出の余地あり）
  virtual std::vector<StateBase *> getDependencyStates() const { return {}; }
  // バインド/購読API
  typedef void (*OnChangeFn)(void *userData);
  virtual void bind(OnChangeFn cb, void *userData, bool callImmediately = true, bool callOnce = false, int priority = 0) {}
  virtual void unbind(OnChangeFn cb, void *userData) {}
  virtual void deferBind(OnChangeFn cb, void *userData, int priority = 0) {}
  virtual void deferUnbind(OnChangeFn cb, void *userData) {}
  // 再計算（DerivedStateでオーバーライド）
  virtual bool recompute() { return false; }
};

// State<T>: StateBaseを継承し、値の保持・購読APIを実装
// bindTrackerは削除

template <typename T>
class State : public StateBase
{
public:
  State(const T &initial = T()) : value(initial) {}
  virtual ~State() {}
  virtual T get() const { return value; }

public:
  void bind(OnChangeFn cb, void *userData, bool callImmediately = true, bool callOnce = false, int priority = 0) override
  {
    Handler h{cb, userData, callOnce, priority};
    auto it = handlers.begin();
    for (; it != handlers.end(); ++it)
      if (priority > it->priority)
        break;
    handlers.insert(it, h);
    if (callImmediately && cb)
    {
      cb(userData);
      if (callOnce)
        unbind(cb, userData);
    }
  }
  void unbind(OnChangeFn cb, void *userData) override
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
  void deferBind(OnChangeFn cb, void *userData, int priority = 0) override
  {
    Handler h{cb, userData, false, priority};
    auto it = deferredHandlers.begin();
    for (; it != deferredHandlers.end(); ++it)
      if (priority > it->priority)
        break;
    deferredHandlers.insert(it, h);
  }
  void deferUnbind(OnChangeFn cb, void *userData) override
  {
    Handler target{cb, userData, false, 0};
    for (size_t i = 0; i < deferredHandlers.size(); ++i)
    {
      if (deferredHandlers[i] == target)
      {
        deferredHandlers.erase(deferredHandlers.begin() + i);
        break;
      }
    }
  }

protected:
  virtual void set(const T &v)
  {
    if (value != v)
    {
      value = v;
      notifyStateChanged();
    }
  }
  virtual void setValue(const T &v) { set(v); }
  // setValue(const ValueHolderBase&)は不要になったので削除
  // バインドAPI: 値が変わったらcbを呼ぶ
  std::vector<StateBase *> getDependencyStates() const override { return {}; }
  // 通知
  void notifyStateChanged()
  {
    for (size_t i = 0; i < handlers.size();)
    {
      handlers[i].cb(handlers[i].userData);
      if (handlers[i].callOnce)
        handlers.erase(handlers.begin() + i);
      else
        ++i;
    }
    // deferBindで登録されたハンドラも呼ぶ
    for (size_t i = 0; i < deferredHandlers.size(); ++i)
    {
      deferredHandlers[i].cb(deferredHandlers[i].userData);
    }
  }

protected:
  struct Handler
  {
    OnChangeFn cb;
    void *userData;
    bool callOnce;
    int priority;
    bool operator==(const Handler &other) const
    {
      return cb == other.cb && userData == other.userData;
    }
  };
  std::vector<Handler> handlers;
  std::vector<Handler> deferredHandlers;
  T value;
};

// --- 新規: MutableState ---
template <typename T>
class MutableState : public State<T>
{
public:
  using State<T>::State;
  using State<T>::setValue;
  void set(const T &v, bool forceUpdate = false)
  {
#ifdef TEST_BUILD
    printf("[MutableState::set] this=%p\n", (void *)this);
#endif
    if (forceUpdate)
    {
      State<T>::set(v);
      this->notifyStateChanged(); // 値が同じでも必ず通知
    }
    else
    {
      State<T>::set(v);
    }
    if (this->currentTracker)
      this->currentTracker->markDirty(this);
  }
};

// --- DerivedState: State<T>の機能を最大限活用して簡素化 ---
template <typename T>
class DerivedState : public State<T>
{
public:
  using EvalFn = std::function<T()>;
  DerivedState(const std::vector<StateBase *> &deps, EvalFn eval)
      : State<T>(eval()), dependencies(deps), evalFn(eval)
  {
    this->value = evalFn();
  }
  DerivedState(StateBase *dep, EvalFn eval)
      : State<T>(eval()), evalFn(eval)
  {
    if (dep)
      dependencies.push_back(dep);
    this->value = evalFn();
  }

  std::vector<StateBase *> getDependencyStates() const override
  {
    return dependencies;
  }
  // setter完全禁止 - オーバーライドではなく隠蔽実装
private:
  // 親クラスを隠蔽、privateで空実装
  void set(const T &v) override {}
  void setValue(const T &v) override {}
  // 再計算（依存元が変化したときに呼ぶ）
  bool recompute() override
  {
    T newVal = evalFn();
    if (newVal != this->value)
    {
      this->value = newVal;
      this->notifyStateChanged();
      return true;
    }
    return false;
  }

private:
  std::vector<StateBase *> dependencies;
  EvalFn evalFn;
};

#endif // DECLARA_STATE_HPP
