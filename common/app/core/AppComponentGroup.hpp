#ifndef LOKA_APP_COMPONENT_GROUP_HPP
#define LOKA_APP_COMPONENT_GROUP_HPP
#include "app/core/AppComponent.hpp"
#include <cstddef>
#include <vector>

class AppComponentGroup
{
public:
  explicit AppComponentGroup(std::vector<AppComponent *> &src)
  {
    components.swap(src);
  }
  explicit AppComponentGroup(std::vector<AppComponent *> src)
  {
    components.swap(src);
  }
  virtual ~AppComponentGroup()
  {
    for (std::size_t i = 0; i < components.size(); ++i)
      delete components[i];
  }
  const std::vector<AppComponent *> &getComponents() const { return components; }

  std::vector<AppComponent *> build()
  {
    std::vector<AppComponent *> tmp;
    tmp.swap(components);
    return tmp;
  }

protected:
  std::vector<AppComponent *> components;

  // コピー禁止
  AppComponentGroup(const AppComponentGroup &);
  AppComponentGroup &operator=(const AppComponentGroup &);
};

#endif // LOKA_APP_COMPONENT_GROUP_HPP
