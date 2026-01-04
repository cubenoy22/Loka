#ifndef LOKA_HELLOWORLD_ROOT_BOUNDARY_HPP
#define LOKA_HELLOWORLD_ROOT_BOUNDARY_HPP

#include <string>
#include "core/State.hpp"
#include "core2/scene/BoundState.hpp"

namespace helloworld
{
  class RootBoundary
  {
  public:
    virtual ~RootBoundary() {}
    virtual declara::core::scene::BoundState<std::string> &messageState() = 0;
  };
} // namespace helloworld

#endif // LOKA_HELLOWORLD_ROOT_BOUNDARY_HPP
