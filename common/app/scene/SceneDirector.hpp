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

          void includeDirtyFlags(NodeDirtyFlags flags)
          {
            effectiveDirtyFlags = static_cast<NodeDirtyFlags>(effectiveDirtyFlags | flags);
          }

          bool hasEffectiveDirtyFlag(NodeDirtyFlags flags) const
          {
            return (effectiveDirtyFlags & flags) != 0;
          }

          void relaxFullRebuild()
          {
            effectiveFullRebuild = false;
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
          struct PendingBoundaryQueue
          {
            PendingBoundaryQueue()
                : head(0), tail(0)
            {
            }

            BoundaryNode *first() const
            {
              return head;
            }

            void append(BoundaryNode *boundary);
            void clearPendingStates();
            void clear();

            BoundaryNode *head;
            BoundaryNode *tail;
          };

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

          struct PendingProjectionWave
          {
            PendingProjectionWave()
                : projection(), generation()
            {
            }

            const SceneProjectionTransaction &projectionTransaction() const
            {
              return projection;
            }

            NodeDirtyFlags aggregateDirtyFlags() const
            {
              return projection.aggregateDirtyFlags();
            }

            void enqueueTarget(Node *node, NodeDirtyFlags flags)
            {
              generation.ensureActive();
              projection.enqueue(node, flags);
            }

            bool hasPending() const
            {
              return projection.hasPending();
            }

            unsigned long pendingGeneration() const
            {
              return hasPending() ? generation.current() : 0;
            }

            void clear()
            {
              projection.clear();
              generation.clear();
            }

            SceneProjectionTransaction projection;
            PendingWaveGeneration generation;
          };

          SceneUpdateTransaction()
              : pendingWave(),
                pendingBoundaries()
          {
          }

          const SceneProjectionTransaction &projectionTransaction() const
          {
            return pendingWave.projectionTransaction();
          }

          NodeDirtyFlags aggregateDirtyFlags() const
          {
            return pendingWave.aggregateDirtyFlags();
          }

          void enqueueProjectionTarget(Node *node, NodeDirtyFlags flags)
          {
            pendingWave.enqueueTarget(node, flags);
          }

          BoundaryNode *firstPendingBoundary() const
          {
            return pendingBoundaries.first();
          }

          bool hasPendingWave() const
          {
            return pendingWave.hasPending();
          }

          unsigned long pendingGeneration() const
          {
            return pendingWave.pendingGeneration();
          }

          void enqueuePendingBoundary(BoundaryNode *boundary);
          void clearPendingState();

          SceneUpdateRequestSnapshot buildRequestSnapshot(Node *rootNode,
                                                         BoundaryNode *firstPendingRoot,
                                                         NodeDirtyFlags requestedDirtyFlags,
                                                         bool requestedFullRebuild) const
          {
            SceneUpdateRequestSnapshot snapshot;
            if (!hasPendingWave())
            {
              return snapshot;
            }
            const NodeDirtyFlags transactionDirtyFlags = aggregateDirtyFlags();
            snapshot.requestedDirtyFlags = requestedDirtyFlags;
            snapshot.transactionDirtyFlags = transactionDirtyFlags;
            snapshot.includeDirtyFlags(requestedDirtyFlags);
            snapshot.includeDirtyFlags(transactionDirtyFlags);
            snapshot.requestedFullRebuild = requestedFullRebuild;
            snapshot.effectiveFullRebuild =
                requestedFullRebuild || snapshot.hasEffectiveDirtyFlag(static_cast<NodeDirtyFlags>(NODE_DIRTY_CHILD | NODE_DIRTY_INITIAL));
            snapshot.firstPendingRoot = firstPendingRoot;
            snapshot.rootBoundary = rootNode ? rootNode->asBoundary() : 0;
            return snapshot;
          }

          void clear()
          {
            pendingWave.clear();
            pendingBoundaries.clear();
          }

          PendingProjectionWave pendingWave;
          PendingBoundaryQueue pendingBoundaries;
        };

        SceneDirector();

        void attach(Scene *scene);
        void detach();

        void registerBoundaryUpdate(BoundaryNode *boundary, NodeDirtyFlags flags);
        void requestBoundaryUpdate(BoundaryNode *boundary, NodeDirtyFlags flags, bool flushImmediately);

        const SceneProjectionTransaction &projectionTransaction() const;
        NodeDirtyFlags aggregateDirtyFlags() const;
        BoundaryNode *firstPendingBoundary() const;
        BoundaryNode *topMostRequestedBoundary(BoundaryNode *boundary) const;
        bool isBoundaryUpdateRoot(BoundaryNode *boundary) const;
        BoundaryNode *firstPendingUpdateRoot() const;
        BoundaryNode *nextPendingUpdateRoot(BoundaryNode *afterRoot) const;
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
#ifdef TEST_BUILD
        unsigned long projectionTransactionGenerationForTesting() const
        {
          return updateTransaction_.pendingGeneration();
        }
#endif

      private:
        void enqueueBoundary(BoundaryNode *boundary);

        Scene *scene_;
        SceneUpdateTransaction updateTransaction_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_SCENEDIRECTOR_HPP
