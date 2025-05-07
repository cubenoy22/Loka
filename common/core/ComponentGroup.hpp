#ifndef DECLARA_COMPONENT_GROUP_HPP
#define DECLARA_COMPONENT_GROUP_HPP
#include <vector>
#include "core/Component.hpp"

class ComponentGroup
{
protected:
  std::vector<Component *> components;

public:
  virtual ~ComponentGroup()
  {
    for (size_t i = 0; i < components.size(); ++i)
      delete components[i];
  }
  const std::vector<Component *> &getComponents() const { return components; }
};

#endif // DECLARA_COMPONENT_GROUP_HPP
