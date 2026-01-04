#ifndef LOKA_HELLOWORLD_ROOT_BOUNDARY_HPP
#define LOKA_HELLOWORLD_ROOT_BOUNDARY_HPP

#include "core/State.hpp"
#include "core2/scene/BoundState.hpp"
#include "loka/core/String.hpp"

namespace helloworld
{
  class RootBoundary
  {
  public:
    virtual ~RootBoundary() {}
    virtual declara::core::scene::BoundState<loka::core::String> &messageState() = 0;
  };
} // namespace helloworld

#endif // LOKA_HELLOWORLD_ROOT_BOUNDARY_HPP
