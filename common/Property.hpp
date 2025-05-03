#ifndef DECLARA_PROPERTY_HPP
#define DECLARA_PROPERTY_HPP

#include <string>

class PropBase
{
public:
  virtual bool recompute() = 0;
  virtual ~PropBase() {}
};

template <typename T>
class State
{
public:
  void set(const T &v)
  {
    if (value != v)
    {
      value = v;
      Transaction::markDirtyDependents(this);
    }
  }
  T get() const { return value; }

private:
  T value;
  friend class Transaction;
};

template <typename T, typename S>
class DerivedProp : public PropBase
{
public:
  typedef T (*EvalFn)(const S &);
  DerivedProp(State<S> *state, EvalFn eval) : source(state), evalFn(eval), onChange(0), onChangeUserData(0)
  {
    cached = eval(source->get());
    Transaction::registerProp(this);
  }
  bool recompute()
  {
    T newVal = evalFn(source->get());
    if (newVal != cached)
    {
      cached = newVal;
      if (onChange)
        onChange(cached, onChangeUserData);
      return true;
    }
    return false;
  }
  void bind(void (*cb)(T, void *), void *userData)
  {
    onChange = cb;
    onChangeUserData = userData;
    if (onChange)
      onChange(cached, onChangeUserData);
  }
  T get() const { return cached; }

private:
  State<S> *source;
  EvalFn evalFn;
  void (*onChange)(T, void *);
  void *onChangeUserData;
  T cached;
};

template <typename T>
class StaticProp : public PropBase
{
public:
  StaticProp(const T &v) : value(v)
  {
    Transaction::registerProp(this);
  }
  bool recompute() { return false; }
  T get() const { return value; }

private:
  T value;
};

extern StaticProp<bool> TRUE;
extern StaticProp<bool> FALSE;

#endif // DECLARA_PROPERTY_HPP
