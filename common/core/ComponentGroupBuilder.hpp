#ifndef DECLARA_COMPONENT_GROUP_BUILDER_HPP
#define DECLARA_COMPONENT_GROUP_BUILDER_HPP
#include <vector>
#include "core/Component.hpp"

class ComponentGroupBuilder
{
protected:
  std::vector<Component *> components;

public:
  virtual ~ComponentGroupBuilder()
  {
    for (size_t i = 0; i < components.size(); ++i)
      delete components[i];
  }
  std::vector<Component *> build()
  {
    std::vector<Component *> tmp;
    tmp.swap(components);
    return tmp;
  }
};

#endif // DECLARA_COMPONENT_GROUP_BUILDER_HPP
