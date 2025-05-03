#ifndef DECLARA_PROPERTY_TYPE_HPP
#define DECLARA_PROPERTY_TYPE_HPP

#include "Property.hpp"
#include <string>

template <typename T>
struct IsBoolProp
{
  enum
  {
    value = false
  };
};

template <>
struct IsBoolProp<StaticProp<bool>>
{
  enum
  {
    value = true
  };
};

template <typename S>
struct IsBoolProp<DerivedProp<bool, S>>
{
  enum
  {
    value = true
  };
};

// 文字列プロパティ型ガード
template <typename T>
struct IsStringProp
{
  enum
  {
    value = false
  };
};
template <>
struct IsStringProp<StaticProp<std::string>>
{
  enum
  {
    value = true
  };
};
template <typename S>
struct IsStringProp<DerivedProp<std::string, S>>
{
  enum
  {
    value = true
  };
};
#define DECLARA_STATIC_STRING_PROP_GUARD(T) \
  char type_must_be_string_prop[IsStringProp<T>::value ? 1 : -1]

#define DECLARA_STATIC_BOOL_PROP_GUARD(T) \
  char type_must_be_bool_prop[IsBoolProp<T>::value ? 1 : -1]

#endif // DECLARA_PROPERTY_TYPE_HPP
