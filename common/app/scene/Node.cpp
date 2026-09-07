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
        // "Composed and not detached" is the boundary edge, not the full
        // attached triple: a scene mounted without a Window (the public
        // Scene::mount(IPlatformController*) path) composes with a null
        // window, and its retained nodes must still follow their props.
        if (!composable || !composable->attached_.boundary_)
        {
          return;
        }
        composable->releaseCallbacks();
        composable->declareBindingsWithToken();
      }
    } // namespace scene
  } // namespace app
} // namespace loka
