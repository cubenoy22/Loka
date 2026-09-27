#include "app/scene/projection/PlatformController.hpp"
#include "app/scene/Scene.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {

      void IPlatformController::requestSceneRelayout(Node *rootNode)
      {
        BoundaryNode *root = rootNode ? rootNode->asBoundary() : 0;
        if (root && root->scene())
          root->scene()->requestLayoutAfterRun();
      }

      bool PrepareProjectedLayout(IPlatformController *controller, Node *node, LayoutState &state)
      {
        if (!controller)
        {
          return false;
        }
        return controller->prepareProjectedLayout(node, state);
      }

    } // namespace scene
  } // namespace app
} // namespace loka
