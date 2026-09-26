#ifndef LOKA_TESTING_APP_COMPOSABLE_NODE_TEST_ACCESS_HPP
#define LOKA_TESTING_APP_COMPOSABLE_NODE_TEST_ACCESS_HPP

#include "app/scene/node/ComposableNode.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      /** Testing-only census of callbacks and state declarations owned by one composable node. */
      class ComposableNodeTestAccess
      {
      public:
        static size_t declaredStateCount(const ComposableNode &node)
        {
          size_t count = 0;
          for (size_t i = 0; i < node.participants_.size(); ++i)
            if (node.participants_[i]->asStateRegistration()) ++count;
          return count;
        }

        static size_t uiCallbackCount(const ComposableNode &node)
        {
          return node.callbacks_.size();
        }
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif
