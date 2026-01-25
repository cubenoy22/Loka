#ifndef LOKA_HELLOWORLD_ROOT_BOUNDARY_HPP
#define LOKA_HELLOWORLD_ROOT_BOUNDARY_HPP

#include "loka/core/State.hpp"
#include "app/scene/BoundState.hpp"
#include "app/scene/Node.hpp"
#include "loka/core/String.hpp"
#include <cstring>

namespace helloworld
{
  class RootBoundary
  {
  public:
    static const char *kInterfaceName() { return "RootBoundary"; }

    static RootBoundary *fromNode(loka::app::scene::Node *node)
    {
      if (!node) return 0;
      return static_cast<RootBoundary *>(node->queryInterface(kInterfaceName()));
    }

    virtual ~RootBoundary() {}
    virtual loka::app::scene::BoundState<loka::core::String> &messageState() = 0;
  };
} // namespace helloworld

#endif // LOKA_HELLOWORLD_ROOT_BOUNDARY_HPP
