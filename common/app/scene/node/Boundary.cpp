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
        if (this->flushViewDirtyImmediately(flags))
        {
          scene->invalidate(flags);
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
          NodeDirtyFlags flags = self->observedDirtyFlagsForCommittedStates();
          if (flags == NODE_DIRTY_NONE)
          {
            flags = self->observedDirtyFlags();
          }
          if (flags == NODE_DIRTY_NONE)
          {
            flags = NODE_DIRTY_PROPS;
          }
          self->markViewDirty(flags);
        }
      }

      void BoundaryNode::ObservedStateChangedThunk(void *userData)
      {
        BoundaryNode::ObservedStateBinding *binding = static_cast<BoundaryNode::ObservedStateBinding *>(userData);
        if (!binding || !binding->boundary)
        {
          return;
        }
        if (binding->state && binding->state->trackerOwner() == binding->boundary->tracker())
        {
          return;
        }
        if (!binding->boundary->getScene())
        {
          return;
        }
        NodeDirtyFlags flags = binding->flags;
        if (flags == NODE_DIRTY_NONE)
        {
          flags = NODE_DIRTY_PROPS;
        }
        loka::core::StateTracker *ownerTracker = binding->state ? binding->state->trackerOwner() : 0;
        if (ownerTracker && ownerTracker->phase() != loka::core::TRACKER_IDLE)
        {
          ownerTracker->defer(&BoundaryNode::ObservedStateDeferredInvalidateThunk, binding);
          return;
        }
        binding->boundary->markViewDirty(flags);
      }

      void BoundaryNode::ObservedStateDeferredInvalidateThunk(void *userData)
      {
        BoundaryNode::ObservedStateBinding *binding = static_cast<BoundaryNode::ObservedStateBinding *>(userData);
        if (!binding || !binding->boundary)
        {
          return;
        }
        NodeDirtyFlags flags = binding->flags;
        if (flags == NODE_DIRTY_NONE)
        {
          flags = NODE_DIRTY_PROPS;
        }
        binding->boundary->markViewDirty(flags);
      }
    } // namespace scene
  } // namespace app
} // namespace loka
