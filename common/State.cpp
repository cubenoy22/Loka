#include "State.hpp"
#include "Tracker.hpp"
#include <string>

// テンプレートの特殊化のための実装
// よく使われる型のみを明示的にインスタンス化
template <>
void State<bool>::notifyStateChanged()
{
  if (tracker_)
    tracker_->markDirty(this);
}

template <>
void State<int>::notifyStateChanged()
{
  if (tracker_)
    tracker_->markDirty(this);
}

template <>
void State<float>::notifyStateChanged()
{
  if (tracker_)
    tracker_->markDirty(this);
}

template <>
void State<double>::notifyStateChanged()
{
  if (tracker_)
    tracker_->markDirty(this);
}

template <>
void State<std::string>::notifyStateChanged()
{
  if (tracker_)
    tracker_->markDirty(this);
}

// 汎用実装 - あらゆる型で共通の実装
// これは最終手段の特殊化（catch-all）としての役割を果たす
template <typename T>
void State<T>::notifyStateChanged()
{
  if (tracker_)
    tracker_->markDirty(this);
}
