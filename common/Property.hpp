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

// --- 構造体まとめ方式: 汎用複数依存元 DerivedPropStruct ---
template <typename T, typename StructType>
class DerivedPropStruct : public BindableProp<T>
{
public:
  typedef T (*EvalFn)(const StructType &);
  DerivedPropStruct(Tracker *tracker, const std::vector<StateBase *> &states, EvalFn eval)
      : tracker_(tracker), states_(states), evalFn_(eval)
  {
    if (tracker_ && !states_.empty())
      tracker_->registerProp(this, states_);
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
    Handler h = {cb, userData, callOnce, priority};
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
    Handler target = {cb, userData, false, 0};
    for (size_t i = 0; i < handlers_.size(); ++i)
      if (handlers_[i] == target)
      {
        handlers_.erase(handlers_.begin() + i);
        break;
      }
  }
  T get() const { return cached_; }

private:
  struct Handler
  {
    typename BindableProp<T>::OnChangeFn fn;
    void *userData;
    bool callOnce;
    int priority;
    bool operator==(const Handler &other) const { return fn == other.fn && userData == other.userData; }
  };
  Tracker *tracker_;
  std::vector<StateBase *> states_;
  EvalFn evalFn_;
  T cached_;
  std::vector<Handler> handlers_;
  StructType getStruct() const
  {
    StructType s;
    fillStruct(s, 0);
    return s;
  }
  // 構造体の各メンバにStateの値をセット（要: StructTypeにpublicメンバ）
  template <int I = 0>
  void fillStruct(StructType &s, int idx) const
  {
    // デフォルト: ユーザーが明示的にspecializeする
  }
};

// --- テンプレート量産方式: DerivedProp2, DerivedProp3 ---
template <typename T, typename S1, typename S2>
class DerivedProp2 : public BindableProp<T>
{
public:
  typedef T (*EvalFn)(const S1 &, const S2 &);
  DerivedProp2(Tracker *tracker, State<S1> *s1, State<S2> *s2, EvalFn eval)
      : tracker_(tracker), s1_(s1), s2_(s2), evalFn_(eval)
  {
    if (tracker_)
    {
      std::vector<StateBase *> deps;
      deps.push_back(s1_);
      deps.push_back(s2_);
      tracker_->registerProp(this, deps);
    }
    cached_ = evalFn_(s1_->get(), s2_->get());
  }
  bool recompute()
  {
    T newVal = evalFn_(s1_->get(), s2_->get());
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
    Handler h = {cb, userData, callOnce, priority};
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
    Handler target = {cb, userData, false, 0};
    for (size_t i = 0; i < handlers_.size(); ++i)
      if (handlers_[i] == target)
      {
        handlers_.erase(handlers_.begin() + i);
        break;
      }
  }
  T get() const { return cached_; }

private:
  struct Handler
  {
    typename BindableProp<T>::OnChangeFn fn;
    void *userData;
    bool callOnce;
    int priority;
    bool operator==(const Handler &other) const { return fn == other.fn && userData == other.userData; }
  };
  Tracker *tracker_;
  State<S1> *s1_;
  State<S2> *s2_;
  EvalFn evalFn_;
  T cached_;
  std::vector<Handler> handlers_;
};

template <typename T, typename S1, typename S2, typename S3>
class DerivedProp3 : public BindableProp<T>
{
public:
  typedef T (*EvalFn)(const S1 &, const S2 &, const S3 &);
  DerivedProp3(Tracker *tracker, State<S1> *s1, State<S2> *s2, State<S3> *s3, EvalFn eval)
      : tracker_(tracker), s1_(s1), s2_(s2), s3_(s3), evalFn_(eval)
  {
    if (tracker_)
    {
      std::vector<StateBase *> deps;
      deps.push_back(s1_);
      deps.push_back(s2_);
      deps.push_back(s3_);
      tracker_->registerProp(this, deps);
    }
    cached_ = evalFn_(s1_->get(), s2_->get(), s3_->get());
  }
  bool recompute()
  {
    T newVal = evalFn_(s1_->get(), s2_->get(), s3_->get());
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
    Handler h = {cb, userData, callOnce, priority};
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
    Handler target = {cb, userData, false, 0};
    for (size_t i = 0; i < handlers_.size(); ++i)
      if (handlers_[i] == target)
      {
        handlers_.erase(handlers_.begin() + i);
        break;
      }
  }
  T get() const { return cached_; }

private:
  struct Handler
  {
    typename BindableProp<T>::OnChangeFn fn;
    void *userData;
    bool callOnce;
    int priority;
    bool operator==(const Handler &other) const { return fn == other.fn && userData == other.userData; }
  };
  Tracker *tracker_;
  State<S1> *s1_;
  State<S2> *s2_;
  State<S3> *s3_;
  EvalFn evalFn_;
  T cached_;
  std::vector<Handler> handlers_;
};

#endif // DECLARA_PROPERTY_HPP
