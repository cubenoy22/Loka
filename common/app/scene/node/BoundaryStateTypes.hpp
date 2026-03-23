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

      class BoundaryComposePhaseScope
      {
      public:
        explicit BoundaryComposePhaseScope(BoundaryPhaseState *state)
            : state_(state), active_(state != 0)
        {
          if (state_)
          {
            state_->beginCompose();
          }
        }

        BoundaryComposePhaseScope(const BoundaryComposePhaseScope &other)
            : state_(other.state_), active_(other.active_)
        {
          const_cast<BoundaryComposePhaseScope &>(other).active_ = false;
        }

        ~BoundaryComposePhaseScope()
        {
          if (active_)
          {
            state_->endCompose();
          }
        }

        void release()
        {
          if (active_)
          {
            state_->endCompose();
            active_ = false;
          }
        }

      private:
        BoundaryComposePhaseScope &operator=(const BoundaryComposePhaseScope &);

        BoundaryPhaseState *state_;
        mutable bool active_;
      };

      class BoundaryApplyPhaseScope
      {
      public:
        explicit BoundaryApplyPhaseScope(BoundaryPhaseState *state)
            : state_(state), active_(state != 0)
        {
          if (state_)
          {
            state_->beginApply();
          }
        }

        BoundaryApplyPhaseScope(const BoundaryApplyPhaseScope &other)
            : state_(other.state_), active_(other.active_)
        {
          const_cast<BoundaryApplyPhaseScope &>(other).active_ = false;
        }

        ~BoundaryApplyPhaseScope()
        {
          if (active_)
          {
            state_->endApply();
          }
        }

        void release()
        {
          if (active_)
          {
            state_->endApply();
            active_ = false;
          }
        }

      private:
        BoundaryApplyPhaseScope &operator=(const BoundaryApplyPhaseScope &);

        BoundaryPhaseState *state_;
        mutable bool active_;
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
          PaintMetadata() : hasPaintWork(false), requiresCompositedPaint(false), hasOpaqueCoverageHint(false), opaqueCoverageHint(false) {}

          void clear()
          {
            hasPaintWork = false;
            requiresCompositedPaint = false;
            hasOpaqueCoverageHint = false;
            opaqueCoverageHint = false;
          }

          bool hasPaintWork;
          bool requiresCompositedPaint;
          bool hasOpaqueCoverageHint;
          bool opaqueCoverageHint;
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

        void noteLocalPaintWork()
        {
          paint.hasPaintWork = true;
        }

        void noteCompositedPaint()
        {
          paint.hasPaintWork = true;
          paint.requiresCompositedPaint = true;
        }

        void noteOpaquePaintCoverage(bool opaque)
        {
          paint.hasPaintWork = true;
          paint.hasOpaqueCoverageHint = true;
          paint.opaqueCoverageHint = opaque;
        }

        void noteActualBoundsChanged(bool affectsAncestor)
        {
          actualBoundsChanged = true;
          if (affectsAncestor)
          {
            affectsAncestorLayout = true;
          }
        }
      };

      struct BoundaryUpdateState
      {
        BoundaryUpdateState() : pending(), result(), phase() {}

        void clearPending()
        {
          pending.clear();
        }

        void clearResult()
        {
          result.clear();
        }

        void clearPhase()
        {
          phase.clear();
        }

        void clearAll()
        {
          pending.clear();
          result.clear();
          phase.clear();
        }

        void noteLocalPaintWork()
        {
          result.noteLocalPaintWork();
        }

        void noteCompositedPaint()
        {
          result.noteCompositedPaint();
        }

        void noteOpaquePaintCoverage(bool opaque)
        {
          result.noteOpaquePaintCoverage(opaque);
        }

        void noteActualBoundsChanged(bool affectsAncestor)
        {
          result.noteActualBoundsChanged(affectsAncestor);
        }

        PendingUpdateState pending;
        BoundaryUpdateResult result;
        BoundaryPhaseState phase;
      };
    }
  }
}

#endif
