#ifndef DECLARA_HELLOWORLD_BOUNDARY_HPP
#define DECLARA_HELLOWORLD_BOUNDARY_HPP

#include <string>
#include "core/State.hpp"

namespace helloworld
{
  class HelloWorldBoundary
  {
  public:
    virtual ~HelloWorldBoundary() {}
    virtual MutableState<std::string> &messageState() = 0;
  };
} // namespace helloworld

#endif // DECLARA_HELLOWORLD_BOUNDARY_HPP
