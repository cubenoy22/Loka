#ifndef DECLARA_COMPONENT_GROUP_BUILDER_HPP
#define DECLARA_COMPONENT_GROUP_BUILDER_HPP
#include <vector>
#include "core/Component.hpp"
#include "core/ComponentGroup.hpp"

template <typename T>
class ComponentGroupBuilder
{
  // T must be derived from Component.
  // C++98 では明示的な型制約は不可のため、下記のダミー関数で型チェックを補助します。
  // T が Component の派生型でない場合、static_cast によりコンパイルエラーとなります。
  void _type_check()
  {
    Component *p = static_cast<T *>(0);
    (void)p;
  }

protected:
  std::vector<T *> components;

public:
  virtual ~ComponentGroupBuilder()
  {
    for (size_t i = 0; i < components.size(); ++i)
      delete components[i];
  }

  ComponentGroup<T> *buildPtr()
  {
    return new ComponentGroup<T>(components);
  }
};

#endif // DECLARA_COMPONENT_GROUP_BUILDER_HPP
