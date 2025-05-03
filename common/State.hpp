#ifndef DECLARA_STATE_HPP
#define DECLARA_STATE_HPP

#include "Property.hpp"
#include <vector>

// State<T> のみ分離

template <typename T>
class State
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

private:
  Transaction *tx_;
  T value;
  friend class Transaction;
};

#endif // DECLARA_STATE_HPP
