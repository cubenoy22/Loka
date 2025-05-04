#ifndef DECLARA_VALUEHOLDER_HPP
#define DECLARA_VALUEHOLDER_HPP

// 型消去用のValueHolder基底
class ValueHolderBase
{
public:
  virtual ~ValueHolderBase() {}
};

template <typename T>
class ValueHolder : public ValueHolderBase
{
public:
  T value;
  ValueHolder(const T &v) : value(v) {}
};

#endif // DECLARA_VALUEHOLDER_HPP
