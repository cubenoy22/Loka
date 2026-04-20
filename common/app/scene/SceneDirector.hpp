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
        struct BoundaryUpdateRequest
        {
          BoundaryUpdateRequest()
              : boundary(0), flags(NODE_DIRTY_NONE), flushImmediately(false)
          {
          }

          BoundaryUpdateRequest(BoundaryNode *b, NodeDirtyFlags f, bool flushNow)
              : boundary(b), flags(f), flushImmediately(flushNow)
          {
          }

          BoundaryNode *boundary;
          NodeDirtyFlags flags;
          bool flushImmediately;
        };

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

          void setRequestedInput(NodeDirtyFlags flags, bool fullRebuild, BoundaryNode *root)
          {
            requestedDirtyFlags = flags;
            requestedFullRebuild = fullRebuild;
            rootBoundary = root;
            includeDirtyFlags(flags);
          }

          void setTransactionDirtyFlags(NodeDirtyFlags flags)
          {
            transactionDirtyFlags = flags;
            includeDirtyFlags(flags);
          }

          void setFirstPendingRoot(BoundaryNode *root)
          {
            firstPendingRoot = root;
          }

          void deriveEffectiveFullRebuild()
          {
            effectiveFullRebuild =
                requestedFullRebuild || hasEffectiveDirtyFlag(static_cast<NodeDirtyFlags>(NODE_DIRTY_CHILD | NODE_DIRTY_INITIAL));
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

          BoundaryNode *primaryRoot() const
          {
            return firstPendingRoot ? firstPendingRoot : rootBoundary;
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

          void setRequirements(bool layout,
                               bool structure,
                               bool compositedPaint,
                               bool opaqueLocalPaint,
                               bool localCompositionDiff)
          {
            requiresLayout = layout;
            requiresStructure = structure;
            requiresCompositedPaint = compositedPaint;
            hasOpaqueLocalPaint = opaqueLocalPaint;
            canApplyLocalCompositionDiff = localCompositionDiff;
          }

          bool layoutRequired() const
          {
            return requiresLayout;
          }

          bool structureRequired() const
          {
            return requiresStructure;
          }

          bool compositedPaintRequired() const
          {
            return requiresCompositedPaint;
          }

          bool opaqueLocalPaintRequired() const
          {
            return hasOpaqueLocalPaint;
          }

          bool localCompositionDiffApplicable() const
          {
            return canApplyLocalCompositionDiff;
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

          void setGeneration(unsigned long value)
          {
            generation = value;
          }

          void setRequest(const SceneUpdateRequestSnapshot &value)
          {
            request = value;
          }

          void setApply(const SceneUpdateApplySnapshot &value)
          {
            apply = value;
          }

          void finalizeAfterApplyAnalysis()
          {
            if (apply.layoutRequired())
            {
              request.includeDirtyFlags(NODE_DIRTY_LAYOUT);
            }
          }

          void relaxEffectiveFullRebuild()
          {
            request.relaxFullRebuild();
          }

          bool hasGeneration() const
          {
            return generation != 0;
          }

          bool requiresStructureChange() const
          {
            return request.hasEffectiveDirtyFlag(NODE_DIRTY_INITIAL) ||
                   (request.hasEffectiveDirtyFlag(NODE_DIRTY_CHILD) &&
                    (request.effectiveFullRebuild || apply.structureRequired()));
          }

          bool requiresLayoutChange() const
          {
            return request.hasEffectiveDirtyFlag(NODE_DIRTY_LAYOUT) || apply.layoutRequired();
          }

          bool hasAnyPaintChange() const
          {
            return request.effectiveDirtyFlags != NODE_DIRTY_NONE;
          }

          bool requiresOpaqueLocalPaint() const
          {
            return apply.opaqueLocalPaintRequired();
          }

          bool requiresCompositedPaint() const
          {
            return apply.compositedPaintRequired();
          }

          unsigned long generation;
          SceneUpdateRequestSnapshot request;
          SceneUpdateApplySnapshot apply;
        };

        struct SceneUpdateTransaction
        {
          struct TransactionSnapshot
          {
            struct RequestedInputState
            {
              RequestedInputState()
                  : dirtyFlags(NODE_DIRTY_NONE),
                    fullRebuild(false)
              {
              }

              void clear()
              {
                dirtyFlags = NODE_DIRTY_NONE;
                fullRebuild = false;
              }

              void include(NodeDirtyFlags flags)
              {
                if (flags == NODE_DIRTY_NONE)
                {
                  flags = NODE_DIRTY_PROPS;
                }
                dirtyFlags = static_cast<NodeDirtyFlags>(dirtyFlags | flags);
                if ((flags & static_cast<NodeDirtyFlags>(NODE_DIRTY_CHILD | NODE_DIRTY_INITIAL)) != 0)
                {
                  fullRebuild = true;
                }
              }

              NodeDirtyFlags effectiveDirtyFlags() const
              {
                return dirtyFlags == NODE_DIRTY_NONE ? NODE_DIRTY_PROPS : dirtyFlags;
              }

              bool hasRequestedInput() const
              {
                return dirtyFlags != NODE_DIRTY_NONE;
              }

              NodeDirtyFlags dirtyFlagsValue() const
              {
                return dirtyFlags;
              }

              bool fullRebuildRequested() const
              {
                return fullRebuild;
              }

            private:
              NodeDirtyFlags dirtyFlags;
              bool fullRebuild;
            };

            TransactionSnapshot()
                : projection(), generation(0), requestedInput()
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
              if (!projection.hasPending())
              {
                ++generation;
                if (generation == 0)
                {
                  generation = 1;
                }
              }
              projection.enqueue(node, flags);
            }

            void enqueueSceneRequest(NodeDirtyFlags flags)
            {
              requestedInput.include(flags);
            }

            bool hasProjectionTargets() const
            {
              return projection.hasPending();
            }

            unsigned long snapshotGeneration() const
            {
              return hasProjectionTargets() ? generation : 0;
            }

            NodeDirtyFlags requestedDirtyFlags() const
            {
              return requestedInput.dirtyFlagsValue();
            }

            NodeDirtyFlags effectiveRequestedDirtyFlags() const
            {
              return requestedInput.effectiveDirtyFlags();
            }

            bool hasRequestedInput() const
            {
              return requestedInput.hasRequestedInput();
            }

            bool requestedFullRebuild() const
            {
              return requestedInput.fullRebuildRequested();
            }

            SceneUpdateRequestSnapshot buildRequestSnapshot(BoundaryNode *rootBoundary,
                                                            BoundaryNode *firstPendingRoot) const
            {
              SceneUpdateRequestSnapshot requestSnapshot;
              if (!hasProjectionTargets())
              {
                return requestSnapshot;
              }
              requestSnapshot.setRequestedInput(effectiveRequestedDirtyFlags(), requestedFullRebuild(), rootBoundary);
              requestSnapshot.setTransactionDirtyFlags(aggregateDirtyFlags());
              requestSnapshot.deriveEffectiveFullRebuild();
              requestSnapshot.setFirstPendingRoot(firstPendingRoot);
              return requestSnapshot;
            }

            void clear()
            {
              projection.clear();
              requestedInput.clear();
            }

          private:
            SceneProjectionTransaction projection;
            unsigned long generation;
            RequestedInputState requestedInput;
          };

          SceneUpdateTransaction()
              : transactionSnapshot()
          {
          }

          const SceneProjectionTransaction &projectionTransaction() const
          {
            return transactionSnapshot.projectionTransaction();
          }

          NodeDirtyFlags aggregateDirtyFlags() const
          {
            return transactionSnapshot.aggregateDirtyFlags();
          }

          void enqueueProjectionTarget(Node *node, NodeDirtyFlags flags)
          {
            transactionSnapshot.enqueueTarget(node, flags);
          }

          BoundaryNode *firstPendingBoundary() const
          {
            return nextPendingBoundaryAfterIdentity(0);
          }

          BoundaryNode *nextPendingBoundary(const BoundaryNode *after) const
          {
            return nextPendingBoundaryAfterIdentity(static_cast<const void *>(after));
          }

          bool hasPendingBoundary(const BoundaryNode *boundary) const
          {
            return pendingDirtyFlagsForBoundary(boundary) != NODE_DIRTY_NONE;
          }

          bool hasProjectionTargets() const
          {
            return transactionSnapshot.hasProjectionTargets();
          }

          unsigned long snapshotGeneration() const
          {
            return transactionSnapshot.snapshotGeneration();
          }

          void enqueueSceneRequest(NodeDirtyFlags flags)
          {
            transactionSnapshot.enqueueSceneRequest(flags);
          }

          NodeDirtyFlags requestedDirtyFlags() const
          {
            return transactionSnapshot.requestedDirtyFlags();
          }

          NodeDirtyFlags effectiveRequestedDirtyFlags() const
          {
            return transactionSnapshot.effectiveRequestedDirtyFlags();
          }

          bool hasRequestedInput() const
          {
            return transactionSnapshot.hasRequestedInput();
          }

          bool requestedFullRebuild() const
          {
            return transactionSnapshot.requestedFullRebuild();
          }

          NodeDirtyFlags pendingDirtyFlagsForBoundary(const BoundaryNode *boundary) const
          {
            return projectionTransaction().dirtyFlagsForTargetIdentity(
                SceneProjectionTransaction::TargetIdentity(static_cast<const void *>(boundary)));
          }

          void enqueueBoundaryUpdate(const BoundaryUpdateRequest &request);
          void clearTransaction();

          SceneUpdateRequestSnapshot buildRequestSnapshot(BoundaryNode *rootBoundary,
                                                         BoundaryNode *firstPendingRoot) const
          {
            return transactionSnapshot.buildRequestSnapshot(rootBoundary, firstPendingRoot);
          }

          void clear()
          {
            transactionSnapshot.clear();
          }

        private:
          BoundaryNode *nextPendingBoundaryAfterIdentity(const void *afterIdentity) const
          {
            bool sawAfter = afterIdentity == 0;
            const SceneProjectionTransaction::TargetEntry *entry = projectionTransaction().targetsHead();
            while (entry)
            {
              BoundaryNode *boundary = entry->node ? entry->node->asBoundary() : 0;
              if (boundary)
              {
                if (sawAfter)
                {
                  return boundary;
                }
                if (static_cast<const void *>(boundary) == afterIdentity)
                {
                  sawAfter = true;
                }
              }
              entry = entry->next;
            }
            return 0;
          }

          TransactionSnapshot transactionSnapshot;
        };

        SceneDirector();

        void attach(Scene *scene);
        void detach();

        void registerBoundaryUpdate(const BoundaryUpdateRequest &request);
        void requestBoundaryUpdate(BoundaryNode *boundary, NodeDirtyFlags flags, bool flushImmediately);

        const SceneProjectionTransaction &projectionTransaction() const;
        NodeDirtyFlags aggregateDirtyFlags() const;
        NodeDirtyFlags requestedDirtyFlags() const;
        NodeDirtyFlags effectiveRequestedDirtyFlags() const;
        bool hasRequestedInput() const;
        bool requestedFullRebuild() const;
        NodeDirtyFlags pendingDirtyFlagsForBoundary(const BoundaryNode *boundary) const;
        bool hasPendingBoundary(const BoundaryNode *boundary) const;
        BoundaryNode *firstPendingBoundary() const;
        BoundaryNode *nextPendingBoundary(const BoundaryNode *boundary) const;
        BoundaryNode *topMostRequestedBoundary(BoundaryNode *boundary) const;
        bool isBoundaryUpdateRoot(BoundaryNode *boundary) const;
        BoundaryNode *firstPendingUpdateRoot() const;
        BoundaryNode *nextPendingUpdateRoot(BoundaryNode *afterRoot) const;
        SceneUpdateApplySnapshot buildApplySnapshot(const Scene *scene) const;
        SceneUpdateSnapshot buildUpdateSnapshot(Node *rootNode,
                                               const Scene *scene) const;
        PlatformApplyPlan buildPlatformApplyPlan(const SceneUpdateSnapshot &snapshot) const;
        PlatformApplyPlan executeApplyPlan(Node *rootNode,
                                          IPlatformController *platformController,
                                          const SceneUpdateSnapshot &snapshot,
                                          NodeDirtyFlags globalDirtyFlags,
                                          bool fullRebuild) const;
        void applyPendingBoundaryUpdates(Node *rootNode,
                                         const PlatformApplyPlan &plan) const;
        bool shouldSkipGlobalChange(IPlatformController *platformController,
                                    const PlatformApplyPlan &plan) const;
        void clearUpdateTransaction();
#ifdef TEST_BUILD
        unsigned long projectionTransactionGenerationForTesting() const
        {
          return updateTransaction_.snapshotGeneration();
        }
#endif

      private:
        struct PendingUpdateRootAnalysis
        {
          PendingUpdateRootAnalysis(const SceneDirector *director,
                                    BoundaryNode *afterRoot);

          bool shouldStart(BoundaryNode *root);
          bool hasEquivalentDescendant(BoundaryNode *root) const;
          bool hasSeenRootBefore(BoundaryNode *boundary, BoundaryNode *root) const;
          bool shouldSkip(BoundaryNode *boundary, BoundaryNode *root) const;

          const SceneDirector *director;
          BoundaryNode *afterRoot;
          bool started;
        };

        BoundaryUpdateRequest normalizeBoundaryUpdateRequest(BoundaryNode *boundary,
                                                            NodeDirtyFlags flags,
                                                            bool flushImmediately) const;
        void applyBoundaryUpdateRequest(const BoundaryUpdateRequest &request) const;

        Scene *scene_;
        SceneUpdateTransaction updateTransaction_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_SCENEDIRECTOR_HPP
