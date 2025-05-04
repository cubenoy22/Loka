#ifndef DECLARA_STATE_HPP
#define DECLARA_STATE_HPP

#include "Property.hpp"
#include "ValueHolder.hpp"
#include <vector>

class Transaction;

class StateBase
{
public:
  virtual ~StateBase() {}
  // 型消去: ValueHolderBase経由で値をセット
  virtual void setValue(const ValueHolderBase &) = 0;
};

template <typename T>
class State : public StateBase
{
public:
  State(Transaction *tx, const T &initial = T()) : tx_(tx), value(initial) {}
  void set(const T &v)
  {
    if (value != v)
    {
      value = v;
      if (tx_)
        tx_->markDirtyDependents(this);
    }
  }
  T get() const { return value; }
  void setValue(const T &v) { value = v; }
  void setValue(const ValueHolderBase &v)
  {
    const ValueHolder<T> *vh = dynamic_cast<const ValueHolder<T> *>(&v);
    if (vh)
      value = vh->value;
  }

private:
  Transaction *tx_;
  T value;
  friend class Transaction;
};

#endif // DECLARA_STATE_HPP
