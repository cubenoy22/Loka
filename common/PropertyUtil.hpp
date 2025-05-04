#ifndef DECLARA_PROPERTY_UTIL_HPP
#define DECLARA_PROPERTY_UTIL_HPP

#include "Property.hpp"

// BindablePropBase* から安全にbool値を取得するユーティリティ関数
// StaticProp<bool>やDerivedProp<bool, S>なら get()、それ以外は true を返す（デフォルト有効）
inline bool getBoolProp(BindablePropBase *prop)
{
  // StaticProp<bool>
  StaticProp<bool> *s = dynamic_cast<StaticProp<bool> *>(prop);
  if (s)
    return s->get();
  // DerivedProp<bool, S>
  // Sは不明なのでdynamic_castで全探索（C++98の限界）
  // ここでは代表的な型だけ例示（必要に応じて拡張）
  DerivedProp<bool, std::string> *d1 = dynamic_cast<DerivedProp<bool, std::string> *>(prop);
  if (d1)
    return d1->get();
  // 他の型も必要なら追加
  return true;
}

#endif // DECLARA_PROPERTY_UTIL_HPP
