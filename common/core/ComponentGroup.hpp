#ifndef DECLARA_COMPONENT_GROUP_HPP
#define DECLARA_COMPONENT_GROUP_HPP
#include <vector>
#include "core/Component.hpp"

template <typename T>
class ComponentGroup
{
  // T must be derived from Component.
  // C++98 では明示的な型制約は不可のため、下記のダミー関数で型チェックを補助します。
  // T が Component の派生型でない場合、static_cast によりコンパイルエラーとなります。
  void _type_check()
  {
    Component *p = static_cast<T *>(0);
    (void)p;
  }

public:
  explicit ComponentGroup(std::vector<T *> &src)
  {
    components.swap(src);
  }
  explicit ComponentGroup(std::vector<T *> &&src)
  {
    components.swap(src);
  }
  virtual ~ComponentGroup()
  {
    for (size_t i = 0; i < components.size(); ++i)
      delete components[i];
  }
  const std::vector<T *> &getComponents() const { return components; }

  std::vector<T *> build()
  {
    std::vector<T *> tmp;
    tmp.swap(components);
    return tmp;
  }

protected:
  std::vector<T *> components;

  // コピー禁止
  ComponentGroup(const ComponentGroup &);
  ComponentGroup &operator=(const ComponentGroup &);
};

#endif // DECLARA_COMPONENT_GROUP_HPP
