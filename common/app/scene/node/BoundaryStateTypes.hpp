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

      enum BoundaryPhase
      {
        BOUNDARY_PHASE_IDLE = 0,
        BOUNDARY_PHASE_COMPOSING = 1,
        BOUNDARY_PHASE_UPDATING = 2,
        BOUNDARY_PHASE_APPLYING = 3
      };

      struct BoundaryPhaseState
      {
        BoundaryPhaseState() : current(BOUNDARY_PHASE_IDLE) {}

        void clear()
        {
          current = BOUNDARY_PHASE_IDLE;
        }

        void beginCompose()
        {
          current = BOUNDARY_PHASE_COMPOSING;
        }

        void endCompose()
        {
          current = BOUNDARY_PHASE_IDLE;
        }

        void beginUpdate()
        {
          current = BOUNDARY_PHASE_UPDATING;
        }

        void endUpdate()
        {
          current = BOUNDARY_PHASE_IDLE;
        }

        void beginApply()
        {
          current = BOUNDARY_PHASE_APPLYING;
        }

        void endApply()
        {
          current = BOUNDARY_PHASE_IDLE;
        }

        bool isIdle() const { return current == BOUNDARY_PHASE_IDLE; }
        bool isComposing() const { return current == BOUNDARY_PHASE_COMPOSING; }
        bool isUpdating() const { return current == BOUNDARY_PHASE_UPDATING; }
        bool isApplying() const { return current == BOUNDARY_PHASE_APPLYING; }

        BoundaryPhase current;
      };

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
        struct PaintMetadata
        {
          PaintMetadata() : hasPaintWork(false), requiresCompositedPaint(false) {}

          void clear()
          {
            hasPaintWork = false;
            requiresCompositedPaint = false;
          }

          bool hasPaintWork;
          bool requiresCompositedPaint;
        };

        BoundaryUpdateResult() : actualBoundsChanged(false), affectsAncestorLayout(false), paint() {}

        void clear()
        {
          actualBoundsChanged = false;
          affectsAncestorLayout = false;
          paint.clear();
        }

        bool actualBoundsChanged;
        bool affectsAncestorLayout;
        PaintMetadata paint;
      };
    }
  }
}

#endif
