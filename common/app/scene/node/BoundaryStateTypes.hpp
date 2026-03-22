#ifndef LOKA_CORE2_SCENE_NODE_BOUNDARYSTATETYPES_HPP
#define LOKA_CORE2_SCENE_NODE_BOUNDARYSTATETYPES_HPP

#include "app/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class BoundaryNode;

      struct PendingUpdateState
      {
        PendingUpdateState() : dirtyFlags(NODE_DIRTY_NONE), requested(false), nextBoundary(0) {}

        void clear()
        {
          dirtyFlags = NODE_DIRTY_NONE;
          requested = false;
          nextBoundary = 0;
        }

        NodeDirtyFlags dirtyFlags;
        bool requested;
        BoundaryNode *nextBoundary;
      };

      struct BoundaryComposeResult
      {
        BoundaryComposeResult() : event(COMPOSE_EVENT_ATTACH), dirtyFlagsSeen(NODE_DIRTY_NONE), composed(false), preservedNativeContexts(false) {}

        void clear()
        {
          event = COMPOSE_EVENT_ATTACH;
          dirtyFlagsSeen = NODE_DIRTY_NONE;
          composed = false;
          preservedNativeContexts = false;
        }

        ComposeEvent event;
        NodeDirtyFlags dirtyFlagsSeen;
        bool composed;
        bool preservedNativeContexts;
      };

      struct BoundaryUpdateResult
      {
        BoundaryUpdateResult() : actualBoundsChanged(false), affectsAncestorLayout(false) {}

        void clear()
        {
          actualBoundsChanged = false;
          affectsAncestorLayout = false;
        }

        bool actualBoundsChanged;
        bool affectsAncestorLayout;
      };
    }
  }
}

#endif
