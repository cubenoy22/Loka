#include "Boundary.hpp"
#include "../Scene.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {
      void BoundaryNode::InvalidateSceneThunk(void *userData)
      {
        BoundaryNode *self = static_cast<BoundaryNode *>(userData);
        if (!self)
        {
          return;
        }
        Scene *scene = self->getScene();
        if (scene)
        {
          scene->invalidate();
        }
      }
    } // namespace scene
  } // namespace core
} // namespace declara
