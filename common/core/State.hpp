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
  virtual void deferBind(OnChangeFn cb, void *userData, int priority = 0) const {}
  virtual void deferUnbind(OnChangeFn cb, void *userData) const {}
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
  void deferBind(OnChangeFn cb, void *userData, int priority = 0) const override
  {
    Handler h{cb, userData, false, priority};
    auto it = deferredHandlers.begin();
    for (; it != deferredHandlers.end(); ++it)
      if (priority > it->priority)
        break;
    const_cast<std::vector<Handler> &>(deferredHandlers).insert(it, h);
  }
  void deferUnbind(OnChangeFn cb, void *userData) const override
  {
    Handler target{cb, userData, false, 0};
    for (size_t i = 0; i < deferredHandlers.size(); ++i)
    {
      if (deferredHandlers[i] == target)
      {
        const_cast<std::vector<Handler> &>(deferredHandlers).erase(deferredHandlers.begin() + i);
        break;
      }
    }
  }

  typedef void (*OnChangeWithOldFn)(T oldValue, T newValue, void *userData);
  void deferBindWithOld(OnChangeWithOldFn cb, void *userData, int priority = 0) const
  {
    OldNewCtx *ctx = new OldNewCtx{this->get(), cb, userData, this};
    this->deferBind(&OldNewThunk, ctx, priority);
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

private:
  struct OldNewCtx
  {
    T lastValue;
    OnChangeWithOldFn cb;
    void *userData;
    const State<T> *state;
  };
  static void OldNewThunk(void *ud)
  {
    OldNewCtx *ctx = static_cast<OldNewCtx *>(ud);
    T newValue = ctx->state->get();
    if (ctx->lastValue != newValue)
    {
      ctx->cb(ctx->lastValue, newValue, ctx->userData);
      ctx->lastValue = newValue;
    }
  }
};

// State<void>特殊化: 値は持たず、イベント伝播専用
// - UIイベント（ボタン押下・通知・トリガー等）や「値を持たない状態変化」を表現するための汎用State
// - EmitterStateなどのイベント系Stateの基底としても利用
// - テスト・アプリ本体・拡張ライブラリ等、あらゆる箇所で「型安全なイベント伝播」を実現
// State<T>と同じバインドAPI・ハンドラ管理を持つ
//
template <>
class State<void> : public StateBase
{
public:
  State() {}
  virtual ~State() {}

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
  void deferBind(OnChangeFn cb, void *userData, int priority = 0) const override
  {
    Handler h{cb, userData, false, priority};
    auto it = deferredHandlers.begin();
    for (; it != deferredHandlers.end(); ++it)
      if (priority > it->priority)
        break;
    const_cast<std::vector<Handler> &>(deferredHandlers).insert(it, h);
  }
  void deferUnbind(OnChangeFn cb, void *userData) const override
  {
    Handler target{cb, userData, false, 0};
    for (size_t i = 0; i < deferredHandlers.size(); ++i)
    {
      if (deferredHandlers[i] == target)
      {
        const_cast<std::vector<Handler> &>(deferredHandlers).erase(deferredHandlers.begin() + i);
        break;
      }
    }
  }

protected:
  // イベント通知API - EmitterStateなどの派生クラスが使用
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
};

// --- EmitterState: 値を持たない純粋なイベントState ---
//
// 設計意図:
// - EmitterStateはOSやプラットフォームのイベント（例:ボタンクリック）をemit()で受けるだけの純粋なイベントState。
// - SceneNodeButton等が持つclickEventは、各プラットフォームのSceneNodeContextがOSコールバックでemit()を呼ぶ。
// - Declara!側ではこのEmitterStateにbindDeferした各方面にイベントが伝播される。
// - EmitterStateは値やフラグを一切持たず、emit()でnotifyStateChanged()を呼ぶだけ。
// - 利用側はemitted()やconsume()等を意識せず、bindDeferで副作用を記述するだけでよい。
class EmitterState : public State<void>
{
public:
  EmitterState() {}
  void emit()
  {
    notifyStateChanged();
  }
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

// --- DerivedState: State<T>のC++98対応実装 ---
template <typename T>
class DerivedState : public State<T>
{
public:
  // 評価式用の純粋仮想基底クラス（C++98対応: operator()はconst参照可）
  struct EvalFn
  {
    virtual ~EvalFn() {}
    virtual T operator()() = 0;
  };
  DerivedState(const std::vector<StateBase *> &deps, EvalFn *eval)
      : State<T>(eval ? (*eval)() : T()), dependencies(deps), evalFn(eval)
  {
    this->value = evalFn ? (*evalFn)() : T();
  }
  DerivedState(StateBase *dep, EvalFn *eval)
      : State<T>(eval ? (*eval)() : T()), evalFn(eval)
  {
    if (dep)
      dependencies.push_back(dep);
    this->value = evalFn ? (*evalFn)() : T();
  }

  std::vector<StateBase *> getDependencyStates() const
  {
    return dependencies;
  }

private:
  void set(const T &v) {}
  void setValue(const T &v) {}
  bool recompute()
  {
    if (!evalFn)
      return false;
    T newVal = (*evalFn)();
    if (newVal != this->value)
    {
      this->value = newVal;
      this->notifyStateChanged();
      return true;
    }
    return false;
  }
  std::vector<StateBase *> dependencies;
  EvalFn *evalFn;
};

#endif // DECLARA_STATE_HPP
