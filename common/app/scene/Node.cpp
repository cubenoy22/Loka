#include "app/scene/node/ComposableNode.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      void Node::bindingsFollowProps()
      {
        ComposableNode *composable = this->asComposable();
        if (!composable || !composable->isAttached_)
        {
          return;
        }
        composable->releaseCallbacks();
        composable->declareBindingsWithToken();
      }
    } // namespace scene
  } // namespace app
} // namespace loka
