#ifndef LOKA_CORE2_SCENE_SCENEDIRECTOR_HPP
#define LOKA_CORE2_SCENE_SCENEDIRECTOR_HPP

#include "app/scene/Node.hpp"
#include "app/scene/SceneProjectionTransaction.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class Scene;
      class BoundaryNode;
      class IPlatformController;
      struct PlatformApplyPlan;

      class SceneDirector
      {
      public:
        struct SceneUpdateRequestSnapshot
        {
          SceneUpdateRequestSnapshot()
              : requestedDirtyFlags(NODE_DIRTY_NONE),
                transactionDirtyFlags(NODE_DIRTY_NONE),
                effectiveDirtyFlags(NODE_DIRTY_NONE),
                requestedFullRebuild(false),
                effectiveFullRebuild(false),
                firstPendingRoot(0),
                rootBoundary(0)
          {
          }

          void clear()
          {
            requestedDirtyFlags = NODE_DIRTY_NONE;
            transactionDirtyFlags = NODE_DIRTY_NONE;
            effectiveDirtyFlags = NODE_DIRTY_NONE;
            requestedFullRebuild = false;
            effectiveFullRebuild = false;
            firstPendingRoot = 0;
            rootBoundary = 0;
          }

          NodeDirtyFlags requestedDirtyFlags;
          NodeDirtyFlags transactionDirtyFlags;
          NodeDirtyFlags effectiveDirtyFlags;
          bool requestedFullRebuild;
          bool effectiveFullRebuild;
          BoundaryNode *firstPendingRoot;
          BoundaryNode *rootBoundary;
        };

        struct SceneUpdateApplySnapshot
        {
          SceneUpdateApplySnapshot()
              : requiresLayout(false),
                requiresStructure(false),
                requiresCompositedPaint(false),
                hasOpaqueLocalPaint(false),
                canApplyLocalCompositionDiff(false)
          {
          }

          void clear()
          {
            requiresLayout = false;
            requiresStructure = false;
            requiresCompositedPaint = false;
            hasOpaqueLocalPaint = false;
            canApplyLocalCompositionDiff = false;
          }

          bool requiresLayout;
          bool requiresStructure;
          bool requiresCompositedPaint;
          bool hasOpaqueLocalPaint;
          bool canApplyLocalCompositionDiff;
        };

        struct SceneUpdateSnapshot
        {
          SceneUpdateSnapshot()
              : generation(0), request(), apply()
          {
          }

          void clear()
          {
            generation = 0;
            request.clear();
            apply.clear();
          }

          unsigned long generation;
          SceneUpdateRequestSnapshot request;
          SceneUpdateApplySnapshot apply;
        };

        struct SceneUpdateTransaction
        {
          struct PendingWaveGeneration
          {
            PendingWaveGeneration()
                : active(0), next(1)
            {
            }

            void ensureActive()
            {
              if (active != 0)
              {
                return;
              }
              active = next++;
            }

            unsigned long current() const
            {
              return active;
            }

            void clear()
            {
              active = 0;
            }

            unsigned long active;
            unsigned long next;
          };

          SceneUpdateTransaction()
              : lastRequestedBoundary(0),
                projection(),
                generation(),
                pendingBoundariesHead(0),
                pendingBoundariesTail(0)
          {
          }

          void beginPendingWave()
          {
            generation.ensureActive();
          }

          unsigned long pendingGeneration() const
          {
            return generation.current();
          }

          void clear()
          {
            lastRequestedBoundary = 0;
            projection.clear();
            generation.clear();
            pendingBoundariesHead = 0;
            pendingBoundariesTail = 0;
          }

          BoundaryNode *lastRequestedBoundary;
          SceneProjectionTransaction projection;
          PendingWaveGeneration generation;
          BoundaryNode *pendingBoundariesHead;
          BoundaryNode *pendingBoundariesTail;
        };

        SceneDirector();

        void attach(Scene *scene);
        void detach();

        void registerBoundaryUpdate(BoundaryNode *boundary, NodeDirtyFlags flags);
        void requestBoundaryUpdate(BoundaryNode *boundary, NodeDirtyFlags flags, bool flushImmediately);

        BoundaryNode *lastRequestedBoundary() const;
        const SceneProjectionTransaction &projectionTransaction() const;
        NodeDirtyFlags aggregateDirtyFlags() const;
        BoundaryNode *pendingBoundariesHead() const;
        BoundaryNode *topMostRequestedBoundary(BoundaryNode *boundary) const;
        bool isBoundaryUpdateRoot(BoundaryNode *boundary) const;
        BoundaryNode *firstPendingUpdateRoot() const;
        BoundaryNode *nextPendingUpdateRoot(BoundaryNode *afterRoot) const;
        unsigned long pendingGeneration() const;
        bool requiresLayout() const;
        bool requiresCompositedPaint() const;
        bool requiresStructure(const Scene *scene) const;
        bool hasOpaqueLocalPaint() const;
        bool canApplyLocalCompositionDiff() const;
        SceneUpdateSnapshot buildUpdateSnapshot(Node *rootNode,
                                                NodeDirtyFlags flags,
                                                bool fullRebuild,
                                                const Scene *scene) const;
        PlatformApplyPlan buildPlatformApplyPlan(const SceneUpdateSnapshot &snapshot) const;
        void applyPendingBoundaryUpdates(Node *rootNode,
                                         const PlatformApplyPlan &plan) const;
        bool shouldSkipGlobalChange(IPlatformController *platformController,
                                    const PlatformApplyPlan &plan) const;
        void clearPendingBoundaryRequest();

      private:
        void enqueueBoundary(BoundaryNode *boundary);

        Scene *scene_;
        SceneUpdateTransaction updateTransaction_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_SCENEDIRECTOR_HPP
