#include "Boundary.hpp"
#include "../Scene.hpp"
#if defined(LOKA_DEBUG_RECOMPOSE) && !defined(LOKA_RETRO68)
#include "loka/platform/DebugLog.hpp"
#endif

namespace loka
{
  namespace app
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
#if defined(LOKA_DEBUG_RECOMPOSE) && !defined(LOKA_RETRO68)
          loka::platform::DebugLogRecomposeTracked(static_cast<void *>(self), static_cast<void *>(scene));
#endif
          scene->invalidate();
        }
      }
    } // namespace scene
  } // namespace app
} // namespace loka
