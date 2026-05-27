#ifndef LOKA_CORE_VECTOR_HPP
#define LOKA_CORE_VECTOR_HPP

#include <cstddef>
#include <vector>

namespace loka
{
  namespace dsl
  {
    template <typename T> class Stream;
  }

  template <typename T> class Vector
  {
  public:
    Vector()
        : values_()
    {
    }

    std::size_t size() const
    {
      return values_.size();
    }
    bool empty() const
    {
      return values_.empty();
    }
    void clear()
    {
      values_.clear();
    }
    void reserve(std::size_t count)
    {
      values_.reserve(count);
    }
    void push_back(const T &value)
    {
      values_.push_back(value);
    }
    void assign(const T *items, std::size_t count)
    {
      values_.clear();
      values_.reserve(count);
      for (std::size_t i = 0; i < count; ++i)
      {
        values_.push_back(items[i]);
      }
    }
    T &operator[](std::size_t index)
    {
      return values_[index];
    }
    const T &operator[](std::size_t index) const
    {
      return values_[index];
    }

    typename std::vector<T>::iterator begin()
    {
      return values_.begin();
    }
    typename std::vector<T>::iterator end()
    {
      return values_.end();
    }
    typename std::vector<T>::const_iterator begin() const
    {
      return values_.begin();
    }
    typename std::vector<T>::const_iterator end() const
    {
      return values_.end();
    }

    template <typename R, typename Mapper> Vector<R> map(const Mapper &mapper) const
    {
      Vector<R> result;
      result.reserve(values_.size());
      for (std::size_t i = 0; i < values_.size(); ++i)
      {
        result.push_back(mapper(values_[i]));
      }
      return result;
    }

    dsl::Stream<T> stream();

  private:
    std::vector<T> values_;
  };
} // namespace loka

#endif // LOKA_CORE_VECTOR_HPP
