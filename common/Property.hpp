#ifndef DECLARA_PROPERTY_HPP
#define DECLARA_PROPERTY_HPP

#include <string>
#include <vector>
#include "BaseTypes.hpp"
#include "State.hpp"   // PropBaseの定義はState.hppにあるので、それを使用
#include "Tracker.hpp" // BindablePropBaseの定義を含むヘッダをインクルード

// ここではPropBaseを再定義せず、State.hppから継承する
// PropBaseはすでにState.hppで定義されている

// PropPriorityはBaseTypes.hppに移動済み

// 型安全なbind/unbindを持つ抽象基底クラス
// PropBaseを継承し、T型のOnChangeFn/void* userDataで統一
// C++98対応

template <typename T>
class BindableProp : public BindablePropBase
{
public:
  typedef void (*OnChangeFn)(T, void *);
  virtual void bind(OnChangeFn cb, void *userData, bool callImmediately = true, bool callOnce = false, int priority = PROP_PRIORITY_NORMAL) = 0;
  virtual void unbind(OnChangeFn cb, void *userData) = 0;
  virtual void deferBind(OnChangeFn cb, void *userData, int priority = PROP_PRIORITY_NORMAL) = 0;
  virtual void deferUnbind(OnChangeFn cb, void *userData) = 0; // 追加：deferUnbind仮想関数
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

    // コンストラクタを追加して初期化を簡略化
    Handler() : fn(NULL), userData(NULL), callOnce(false), priority(0) {}

    bool operator==(const Handler &other) const
    {
      return fn == other.fn && userData == other.userData;
    }
  };
  std::vector<Handler> handlers;
  std::vector<Handler> deferredHandlers;

  DerivedProp(State<S> *state, EvalFn eval)
      : source(state), evalFn(eval)
  {
    cached = eval(source->get());
  }

  DerivedProp(const std::vector<State<S> *> &states, EvalFn eval)
      : source(states.empty() ? 0 : states[0]), evalFn(eval)
  {
    if (source)
      cached = eval(source->get());
  }

  bool recompute()
  {
    T newVal = evalFn(source->get());
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
      // commitフェーズ時のみdeferredHandlersを発火
      // (Trackerの管理下で明示的に呼ぶ設計に変更)
      return true;
    }
    return false;
  }
  void bind(OnChangeFn cb, void *userData, bool callImmediately = true, bool callOnce = false, int priority = PROP_PRIORITY_NORMAL)
  {
    Handler h;
    h.fn = cb;
    h.userData = userData;
    h.callOnce = callOnce;
    h.priority = priority;

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
    Handler target;
    target.fn = cb;
    target.userData = userData;
    target.callOnce = false;
    target.priority = 0;

    for (size_t i = 0; i < handlers.size(); ++i)
    {
      if (handlers[i] == target)
      {
        handlers.erase(handlers.begin() + i);
        break;
      }
    }
  }
  void deferBind(OnChangeFn cb, void *userData, int priority = PROP_PRIORITY_NORMAL)
  {
    Handler h;
    h.fn = cb;
    h.userData = userData;
    h.callOnce = false;
    h.priority = priority;

    std::vector<Handler>::iterator it = deferredHandlers.begin();
    for (; it != deferredHandlers.end(); ++it)
    {
      if (priority > it->priority)
        break;
    }
    deferredHandlers.insert(it, h);
  }
  void deferUnbind(OnChangeFn cb, void *userData)
  {
    Handler target;
    target.fn = cb;
    target.userData = userData;
    target.callOnce = false;
    target.priority = 0;

    for (size_t i = 0; i < deferredHandlers.size(); ++i)
    {
      if (deferredHandlers[i] == target)
      {
        deferredHandlers.erase(deferredHandlers.begin() + i);
        break;
      }
    }
  }
  T get() const { return cached; }
  std::vector<StateBase *> getDependencyStates() const override
  {
    std::vector<StateBase *> deps;
    if (source)
      deps.push_back(source);
    return deps;
  }

private:
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
    }
  }
  void unbind(OnChangeFn cb, void *userData)
  {
    // 何もしない
  }

  // BindablePropBaseから継承した純粋仮想関数の実装
  std::vector<StateBase *> getDependencyStates() const override
  {
    // StaticPropは依存を持たないので空を返す
    return std::vector<StateBase *>();
  }

  // BindableProp<T>から継承した純粋仮想関数の実装
  void deferBind(OnChangeFn cb, void *userData, int priority = PROP_PRIORITY_NORMAL) override
  {
    // StaticPropは値が変わらないので何もしない
  }
  void deferUnbind(OnChangeFn cb, void *userData) override
  {
    // 何もしない
  }

private:
  T value;
};

// TRUEとFALSEはmain.cppで定義されている
// extern宣言をコメントアウトして問題を回避
// extern StaticProp<bool> TRUE;
// extern StaticProp<bool> FALSE;

// --- 構造体まとめ方式: 汎用複数依存元 DerivedPropStruct ---
template <typename T, typename StructType>
class DerivedPropStruct : public BindableProp<T>
{
public:
  typedef T (*EvalFn)(const StructType &);

  // 旧設計（Tracker*受け取り）
  DerivedPropStruct(Tracker *tracker, const std::vector<StateBase *> &states, EvalFn eval)
      : tracker_(tracker), states_(states), evalFn_(eval)
  {
    if (tracker_ && !states_.empty())
      tracker_->registerProp(this, states_);
    cached_ = evalFn_(getStruct());
  }

  // 新設計（Tracker*なし）
  DerivedPropStruct(const std::vector<StateBase *> &states, EvalFn eval)
      : tracker_(nullptr), states_(states), evalFn_(eval)
  {
    cached_ = evalFn_(getStruct());
  }

  bool recompute()
  {
    T newVal = evalFn_(getStruct());
    if (newVal != cached_)
    {
      cached_ = newVal;
      for (size_t i = 0; i < handlers_.size();)
      {
        handlers_[i].fn(cached_, handlers_[i].userData);
        if (handlers_[i].callOnce)
          handlers_.erase(handlers_.begin() + i);
        else
          ++i;
      }
      return true;
    }
    return false;
  }
  void bind(typename BindableProp<T>::OnChangeFn cb, void *userData, bool callImmediately = true, bool callOnce = false, int priority = PROP_PRIORITY_NORMAL)
  {
    Handler h;
    h.fn = cb;
    h.userData = userData;
    h.callOnce = callOnce;
    h.priority = priority;

    std::vector<Handler>::iterator it = handlers_.begin();
    for (; it != handlers_.end(); ++it)
      if (priority > it->priority)
        break;
    handlers_.insert(it, h);
    if (callImmediately && cb)
    {
      cb(cached_, userData);
      if (callOnce)
        unbind(cb, userData);
    }
  }
  void unbind(typename BindableProp<T>::OnChangeFn cb, void *userData)
  {
    Handler target;
    target.fn = cb;
    target.userData = userData;
    target.callOnce = false;
    target.priority = 0;

    for (size_t i = 0; i < handlers_.size(); ++i)
      if (handlers_[i] == target)
      {
        handlers_.erase(handlers_.begin() + i);
        break;
      }
  }
  void deferBind(typename BindableProp<T>::OnChangeFn cb, void *userData, int priority = PROP_PRIORITY_NORMAL)
  {
    Handler h;
    h.fn = cb;
    h.userData = userData;
    h.callOnce = false;
    h.priority = priority;

    std::vector<Handler>::iterator it = deferredHandlers_.begin();
    for (; it != deferredHandlers_.end(); ++it)
      if (priority > it->priority)
        break;
    deferredHandlers_.insert(it, h);
  }
  void deferUnbind(typename BindableProp<T>::OnChangeFn cb, void *userData)
  {
    Handler target;
    target.fn = cb;
    target.userData = userData;
    target.callOnce = false;
    target.priority = 0;

    for (size_t i = 0; i < deferredHandlers_.size(); ++i)
      if (deferredHandlers_[i] == target)
      {
        deferredHandlers_.erase(deferredHandlers_.begin() + i);
        break;
      }
  }
  T get() const { return cached_; }
  std::vector<StateBase *> getDependencyStates() const override
  {
    // 全依存State配列を返す
    return states_;
  }

private:
  struct Handler
  {
    typename BindableProp<T>::OnChangeFn fn;
    void *userData;
    bool callOnce;
    int priority;

    // コンストラクタを追加して初期化を簡略化
    Handler() : fn(NULL), userData(NULL), callOnce(false), priority(0) {}

    // 演算子==を定義
    bool operator==(const Handler &other) const
    {
      return fn == other.fn && userData == other.userData;
    }
  };
  Tracker *tracker_;
  std::vector<StateBase *> states_;
  EvalFn evalFn_;
  T cached_;
  std::vector<Handler> handlers_;
  std::vector<Handler> deferredHandlers_;
  virtual StructType getStruct() const
  {
    StructType s;
    // テンプレート引数を明示的に指定
    this->template fillStruct<0>(s, 0);
    return s;
  }
  // 構造体の各メンバにStateの値をセット（要: StructTypeにpublicメンバ）
  template <int I>
  void fillStruct(StructType &s, int idx) const
  {
    // デフォルト: ユーザーが明示的にspecializeする
  }
};

#endif // DECLARA_PROPERTY_HPP
