#ifndef LOKA_HELLOWORLD_BOUNDARY_HPP
#define LOKA_HELLOWORLD_BOUNDARY_HPP

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

#endif // LOKA_HELLOWORLD_BOUNDARY_HPP
