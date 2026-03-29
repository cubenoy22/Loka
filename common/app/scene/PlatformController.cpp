#include "app/scene/PlatformController.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {

      bool PrepareProjectedLayout(IPlatformController *controller, Node *node, LayoutState &state)
      {
        if (!controller)
        {
          return false;
        }
        return controller->prepareProjectedLayout(node, state);
      }

    }
  }
}
