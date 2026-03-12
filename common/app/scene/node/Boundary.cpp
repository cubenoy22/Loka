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
      void BoundaryNode::markViewDirty(NodeDirtyFlags flags)
      {
        Scene *scene = this->getScene();
        if (!scene)
        {
          return;
        }
        scene->requestInvalidate(flags);
      }

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
          NodeDirtyFlags flags = self->observedDirtyFlags();
          if (flags == NODE_DIRTY_NONE)
          {
            flags = NODE_DIRTY_PROPS;
          }
          self->markViewDirty(flags);
        }
      }
    } // namespace scene
  } // namespace app
} // namespace loka
