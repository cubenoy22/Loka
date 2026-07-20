#ifndef LOKA_CORE2_SCENE_BOUNDARY_BOUNDARYSTATETYPES_HPP
#define LOKA_CORE2_SCENE_BOUNDARY_BOUNDARYSTATETYPES_HPP

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
        BoundaryPhaseState()
            : current(BOUNDARY_PHASE_IDLE)
        {
        }

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

        bool isIdle() const
        {
          return current == BOUNDARY_PHASE_IDLE;
        }
        bool isComposing() const
        {
          return current == BOUNDARY_PHASE_COMPOSING;
        }
        bool isUpdating() const
        {
          return current == BOUNDARY_PHASE_UPDATING;
        }
        bool isApplying() const
        {
          return current == BOUNDARY_PHASE_APPLYING;
        }

        BoundaryPhase current;
      };

      class BoundaryComposePhaseScope
      {
      public:
        explicit BoundaryComposePhaseScope(BoundaryPhaseState *state)
            : state_(state),
              active_(state != 0)
        {
          if (state_)
          {
            state_->beginCompose();
          }
        }

        BoundaryComposePhaseScope(const BoundaryComposePhaseScope &other)
            : state_(other.state_),
              active_(other.active_)
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
            : state_(state),
              active_(state != 0)
        {
          if (state_)
          {
            state_->beginApply();
          }
        }

        BoundaryApplyPhaseScope(const BoundaryApplyPhaseScope &other)
            : state_(other.state_),
              active_(other.active_)
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

      struct BoundaryComposeResult
      {
        BoundaryComposeResult()
            : event(COMPOSE_EVENT_ATTACH),
              dirtyFlagsSeen(NODE_DIRTY_NONE),
              composed(false),
              preservedNativeContexts(false),
              allocationFailed(false)
        {
        }

        void clear()
        {
          event = COMPOSE_EVENT_ATTACH;
          dirtyFlagsSeen = NODE_DIRTY_NONE;
          composed = false;
          preservedNativeContexts = false;
          allocationFailed = false;
        }

        ComposeEvent event;
        NodeDirtyFlags dirtyFlagsSeen;
        bool composed;
        bool preservedNativeContexts;
        /** Allocation white flag (#132 ruling 3): raised inside this compose
            window when a state or node failed to materialize because the
            backend gave up. Compose completion converts it into a projection
            failure instead of completing normally. */
        bool allocationFailed;
      };

      struct BoundaryUpdateResult
      {
        struct BoundsHint
        {
          BoundsHint()
              : x(0),
                y(0),
                width(0),
                height(0),
                valid(false)
          {
          }

          void clear()
          {
            x = 0;
            y = 0;
            width = 0;
            height = 0;
            valid = false;
          }

          void set(int nextX, int nextY, int nextWidth, int nextHeight)
          {
            x = nextX;
            y = nextY;
            width = nextWidth;
            height = nextHeight;
            valid = true;
          }

          int x;
          int y;
          int width;
          int height;
          bool valid;
        };

        struct PaintMetadata
        {
          PaintMetadata()
              : hasPaintWork(false),
                requiresCompositedPaint(false),
                hasOpaqueCoverageHint(false),
                opaqueCoverageHint(false)
          {
          }

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

        BoundaryUpdateResult()
            : actualBoundsChanged(false),
              affectsAncestorLayout(false),
              bounds(),
              paintBounds(),
              paint()
        {
        }

        void clear()
        {
          actualBoundsChanged = false;
          affectsAncestorLayout = false;
          bounds.clear();
          paintBounds.clear();
          paint.clear();
        }

        bool actualBoundsChanged;
        bool affectsAncestorLayout;
        BoundsHint bounds;
        BoundsHint paintBounds;
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

        bool hasPaintWork() const
        {
          return paint.hasPaintWork;
        }

        bool requiresCompositedPaint() const
        {
          return paint.requiresCompositedPaint;
        }

        bool hasOpaqueCoverageHint() const
        {
          return paint.hasOpaqueCoverageHint;
        }

        bool opaqueCoverageHintValue() const
        {
          return paint.opaqueCoverageHint;
        }

        void noteActualBoundsChanged(bool affectsAncestor)
        {
          actualBoundsChanged = true;
          if (affectsAncestor)
          {
            affectsAncestorLayout = true;
          }
        }

        void noteBoundsHint(int x, int y, int width, int height)
        {
          bounds.set(x, y, width, height);
        }

        void clearBoundsHint()
        {
          bounds.clear();
        }

        bool hasBoundsHint() const
        {
          return bounds.valid;
        }

        const BoundsHint *boundsHint() const
        {
          if (!bounds.valid)
          {
            return 0;
          }
          return &bounds;
        }

        void notePaintBoundsHint(int x, int y, int width, int height)
        {
          paintBounds.set(x, y, width, height);
        }

        void clearPaintBoundsHint()
        {
          paintBounds.clear();
        }

        bool hasPaintBoundsHint() const
        {
          return paintBounds.valid;
        }

        const BoundsHint *paintBoundsHint() const
        {
          if (!paintBounds.valid)
          {
            return 0;
          }
          return &paintBounds;
        }
      };

      struct BoundaryUpdateState
      {
        BoundaryUpdateState()
            : result(),
              phase()
        {
        }

        BoundaryUpdateResult &updateResult()
        {
          return result;
        }

        const BoundaryUpdateResult &updateResult() const
        {
          return result;
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

        void noteLayoutBoundsTransition(bool changed, bool affectsAncestor, int x, int y, int width, int height)
        {
          noteLayoutAndPaintBoundsHint(x, y, width, height);
          if (changed)
          {
            result.noteActualBoundsChanged(affectsAncestor);
          }
        }

        void noteLayoutBoundsCleared(bool changed, bool affectsAncestor)
        {
          clearAllBoundsHints();
          if (changed)
          {
            result.noteActualBoundsChanged(affectsAncestor);
          }
        }

        void noteBoundsHint(int x, int y, int width, int height)
        {
          result.noteBoundsHint(x, y, width, height);
        }

        void noteLayoutAndPaintBoundsHint(int x, int y, int width, int height)
        {
          result.noteBoundsHint(x, y, width, height);
          result.notePaintBoundsHint(x, y, width, height);
        }

        void clearBoundsHint()
        {
          result.clearBoundsHint();
        }

        bool hasBoundsHint() const
        {
          return result.hasBoundsHint();
        }

        const BoundaryUpdateResult::BoundsHint *boundsHint() const
        {
          return result.boundsHint();
        }

        void notePaintBoundsHint(int x, int y, int width, int height)
        {
          result.notePaintBoundsHint(x, y, width, height);
        }

        void clearPaintBoundsHint()
        {
          result.clearPaintBoundsHint();
        }

        void clearAllBoundsHints()
        {
          result.clearBoundsHint();
          result.clearPaintBoundsHint();
        }

        bool hasPaintBoundsHint() const
        {
          return result.hasPaintBoundsHint();
        }

        const BoundaryUpdateResult::BoundsHint *paintBoundsHint() const
        {
          return result.paintBoundsHint();
        }

        void selectLocalApplyBoundsHint(bool hasPaintWork,
                                        bool hasLayoutWork,
                                        const BoundaryUpdateResult::BoundsHint *&boundsOut,
                                        bool &usesPaintBoundsHintOut,
                                        bool &hasPaintSpecificBoundsHintOut) const
        {
          hasPaintSpecificBoundsHintOut = hasPaintWork && result.hasPaintBoundsHint();
          usesPaintBoundsHintOut = hasPaintWork && hasPaintSpecificBoundsHintOut;
          boundsOut = 0;
          if (usesPaintBoundsHintOut)
          {
            boundsOut = result.paintBoundsHint();
            return;
          }
          if (hasLayoutWork || hasPaintWork)
          {
            boundsOut = result.boundsHint();
          }
        }

        bool hasPaintWork() const
        {
          return result.hasPaintWork();
        }

        bool requiresCompositedPaint() const
        {
          return result.requiresCompositedPaint();
        }

        bool hasOpaqueCoverageHint() const
        {
          return result.hasOpaqueCoverageHint();
        }

        bool opaqueCoverageHintValue() const
        {
          return result.opaqueCoverageHintValue();
        }

        void selectLocalOpaqueCoverageHint(bool isLocalPaintRoot,
                                           bool &hasOpaqueCoverageHintOut,
                                           bool &opaqueCoverageHintValueOut) const
        {
          hasOpaqueCoverageHintOut = isLocalPaintRoot && result.hasOpaqueCoverageHint();
          opaqueCoverageHintValueOut = hasOpaqueCoverageHintOut && result.opaqueCoverageHintValue();
        }

        bool isApplying() const
        {
          return phase.isApplying();
        }

        bool isComposing() const
        {
          return phase.isComposing();
        }

        void beginApply()
        {
          phase.beginApply();
        }

        void endApply()
        {
          phase.endApply();
        }

        BoundaryComposePhaseScope beginComposeScope()
        {
          return BoundaryComposePhaseScope(&phase);
        }

        BoundaryApplyPhaseScope beginApplyScope()
        {
          return BoundaryApplyPhaseScope(&phase);
        }

        bool canMutateLocalPaintMetadata() const
        {
          return !phase.isApplying();
        }

        BoundaryUpdateResult result;
        BoundaryPhaseState phase;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif
