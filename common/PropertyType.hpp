#ifndef DECLARA_PROPERTY_TYPE_HPP
#define DECLARA_PROPERTY_TYPE_HPP

#include "Property.hpp"

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

#define DECLARA_STATIC_BOOL_PROP_GUARD(T) \
  char type_must_be_bool_prop[IsBoolProp<T>::value ? 1 : -1]

#endif // DECLARA_PROPERTY_TYPE_HPP
