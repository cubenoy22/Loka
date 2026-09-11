#ifndef LOKA_TESTING_APP_COMPOSABLE_NODE_TEST_ACCESS_HPP
#define LOKA_TESTING_APP_COMPOSABLE_NODE_TEST_ACCESS_HPP

#include "app/scene/node/ComposableNode.hpp"

namespace loka { namespace app { namespace scene {
/** Testing-only census of callbacks owned by one composable node. */
class ComposableNodeTestAccess
{
public:
  static size_t uiCallbackCount(const ComposableNode &node)
  {
    return node.callbacks_.size();
  }
};
} } }

#endif
