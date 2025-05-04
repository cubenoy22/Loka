#ifndef DECLARA_STATE_HPP
#define DECLARA_STATE_HPP

#include "BaseTypes.hpp" // BaseTypesから基本クラスをインポート
#include "ValueHolder.hpp"
#include <vector>

class Tracker; // 前方宣言

template <typename T>
class State : public StateBase
{
public:
  State(const T &initial = T()) : value(initial), tracker_(nullptr) {}
  void set(const T &v)
  {
    if (value != v)
    {
      value = v;
      notifyStateChanged();
    }
  }
  T get() const { return value; }
  void setValue(const T &v) { set(v); }
  void setValue(const ValueHolderBase &v) override
  {
    const ValueHolder<T> *vh = dynamic_cast<const ValueHolder<T> *>(&v);
    if (vh)
      set(vh->value);
  }
  void bindTracker(Tracker *tracker) override { tracker_ = tracker; }

  // トラッカー通知関数のインライン実装を避け、前方宣言のみに
  void notifyStateChanged();

private:
  Tracker *tracker_;
  T value;
};

#endif // DECLARA_STATE_HPP
