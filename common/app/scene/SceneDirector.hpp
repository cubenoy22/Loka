#ifndef LOKA_CORE2_SCENE_SCENEDIRECTOR_HPP
#define LOKA_CORE2_SCENE_SCENEDIRECTOR_HPP

#include "app/scene/Node.hpp"
#include "app/scene/SceneProjectionTransaction.hpp"

namespace loka
{
  namespace dsl
  {
    namespace testing
    {
      class SceneTestAccess;
    }
  }

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
          struct TransactionInputs
          {
            TransactionInputs(NodeDirtyFlags requestedFlags,
                              bool requestedRebuild,
                              BoundaryNode *root,
                              NodeDirtyFlags transactionFlags,
                              BoundaryNode *firstRoot)
                : requestedDirtyFlags(requestedFlags),
                  requestedFullRebuild(requestedRebuild),
                  rootBoundary(root),
                  transactionDirtyFlags(transactionFlags),
                  firstPendingRoot(firstRoot)
            {
            }

            NodeDirtyFlags requestedDirtyFlags;
            bool requestedFullRebuild;
            BoundaryNode *rootBoundary;
            NodeDirtyFlags transactionDirtyFlags;
            BoundaryNode *firstPendingRoot;
          };

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

          void captureTransactionInputs(const TransactionInputs &inputs)
          {
            setRequestedInput(inputs.requestedDirtyFlags, inputs.requestedFullRebuild, inputs.rootBoundary);
            setTransactionDirtyFlags(inputs.transactionDirtyFlags);
            deriveEffectiveFullRebuild();
            setFirstPendingRoot(inputs.firstPendingRoot);
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

          NodeDirtyFlags effectiveDirtyFlagsValue() const
          {
            return effectiveDirtyFlags;
          }

          NodeDirtyFlags requestedDirtyFlagsValue() const
          {
            return requestedDirtyFlags;
          }

          NodeDirtyFlags transactionDirtyFlagsValue() const
          {
            return transactionDirtyFlags;
          }

          bool requestedFullRebuildValue() const
          {
            return requestedFullRebuild;
          }

          bool effectiveFullRebuildRequired() const
          {
            return effectiveFullRebuild;
          }

          BoundaryNode *firstPendingRootValue() const
          {
            return firstPendingRoot;
          }

          BoundaryNode *rootBoundaryValue() const
          {
            return rootBoundary;
          }

        private:
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

        private:
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
                    (request.effectiveFullRebuildRequired() || apply.structureRequired()));
          }

          bool requiresLayoutChange() const
          {
            return request.hasEffectiveDirtyFlag(NODE_DIRTY_LAYOUT) || apply.layoutRequired();
          }

          bool hasAnyPaintChange() const
          {
            return request.effectiveDirtyFlagsValue() != NODE_DIRTY_NONE;
          }

          bool requiresOpaqueLocalPaint() const
          {
            return apply.opaqueLocalPaintRequired();
          }

          bool requiresCompositedPaint() const
          {
            return apply.compositedPaintRequired();
          }

          unsigned long generationValue() const
          {
            return generation;
          }

          const SceneUpdateRequestSnapshot &requestSnapshot() const
          {
            return request;
          }

          const SceneUpdateApplySnapshot &applySnapshot() const
          {
            return apply;
          }

        private:
          unsigned long generation;
          SceneUpdateRequestSnapshot request;
          SceneUpdateApplySnapshot apply;
        };

        struct SceneUpdateTransaction
        {
          struct AccumulatedState
          {
            struct AccumulatedProjectionState
            {
              AccumulatedProjectionState()
                  : projection(), generation(0)
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

              void enqueue(Node *node, NodeDirtyFlags flags)
              {
                if (!projection.hasPending())
                {
                  markNewAccumulation();
                }
                projection.enqueue(node, flags);
              }

              bool hasPending() const
              {
                return projection.hasPending();
              }

              unsigned long snapshotGeneration() const
              {
                return hasPending() ? generation : 0;
              }

              void clear()
              {
                projection.clear();
              }

            private:
              void markNewAccumulation()
              {
                ++generation;
                if (generation == 0)
                {
                  generation = 1;
                }
              }

              SceneProjectionTransaction projection;
              unsigned long generation;
            };

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

              NodeDirtyFlags dirtyFlagsForSnapshot() const
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

            AccumulatedState()
                : projectionState(), requestedInput()
            {
            }

            const SceneProjectionTransaction &projectionTransaction() const
            {
              return projectionState.projectionTransaction();
            }

            NodeDirtyFlags aggregateDirtyFlags() const
            {
              return projectionState.aggregateDirtyFlags();
            }

            void enqueueProjectionTarget(Node *node, NodeDirtyFlags flags)
            {
              projectionState.enqueue(node, flags);
            }

            void enqueueRequestedInput(NodeDirtyFlags flags)
            {
              requestedInput.include(flags);
            }

            void enqueueBoundaryUpdate(const BoundaryUpdateRequest &request);

            bool hasAccumulatedUpdates() const
            {
              return projectionState.hasPending();
            }

            unsigned long snapshotGeneration() const
            {
              return projectionState.snapshotGeneration();
            }

            NodeDirtyFlags requestedDirtyFlags() const
            {
              return requestedInput.dirtyFlagsValue();
            }

            NodeDirtyFlags effectiveRequestedDirtyFlags() const
            {
              return requestedInput.dirtyFlagsForSnapshot();
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
              if (!hasAccumulatedUpdates())
              {
                return requestSnapshot;
              }
              SceneUpdateRequestSnapshot::TransactionInputs inputs(effectiveRequestedDirtyFlags(),
                                                                   requestedFullRebuild(),
                                                                   rootBoundary,
                                                                   aggregateDirtyFlags(),
                                                                   firstPendingRoot);
              requestSnapshot.captureTransactionInputs(inputs);
              return requestSnapshot;
            }

            void clearAccumulatedState()
            {
              projectionState.clear();
              requestedInput.clear();
            }

          private:
            AccumulatedProjectionState projectionState;
            RequestedInputState requestedInput;
          };

          SceneUpdateTransaction()
              : accumulatedState()
          {
          }

          const SceneProjectionTransaction &projectionTransaction() const
          {
            return accumulatedState.projectionTransaction();
          }

          NodeDirtyFlags aggregateDirtyFlags() const
          {
            return accumulatedState.aggregateDirtyFlags();
          }

          BoundaryNode *firstPendingBoundary() const
          {
            SceneProjectionTransaction::ConstIterator it = projectionTransaction().targetsBegin();
            while (it.isValid())
            {
              Node *node = it.node();
              BoundaryNode *boundary = node ? node->asBoundary() : 0;
              if (boundary)
              {
                return boundary;
              }
              it.next();
            }
            return 0;
          }

          bool hasPendingBoundary(const BoundaryNode *boundary) const
          {
            return pendingDirtyFlagsForBoundary(boundary) != NODE_DIRTY_NONE;
          }

          bool hasAccumulatedUpdates() const
          {
            return accumulatedState.hasAccumulatedUpdates();
          }

          unsigned long snapshotGeneration() const
          {
            return accumulatedState.snapshotGeneration();
          }

          NodeDirtyFlags requestedDirtyFlags() const
          {
            return accumulatedState.requestedDirtyFlags();
          }

          NodeDirtyFlags effectiveRequestedDirtyFlags() const
          {
            return accumulatedState.effectiveRequestedDirtyFlags();
          }

          bool hasRequestedInput() const
          {
            return accumulatedState.hasRequestedInput();
          }

          bool requestedFullRebuild() const
          {
            return accumulatedState.requestedFullRebuild();
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
            return accumulatedState.buildRequestSnapshot(rootBoundary, firstPendingRoot);
          }

          void clearAccumulatedState()
          {
            accumulatedState.clearAccumulatedState();
          }

        private:
          AccumulatedState accumulatedState;
        };

        SceneDirector();

        void attach(Scene *scene);
        void detach();

        void registerBoundaryUpdate(const BoundaryUpdateRequest &request);
        void requestBoundaryUpdate(BoundaryNode *boundary, NodeDirtyFlags flags, bool flushImmediately);

        NodeDirtyFlags pendingDirtyFlagsForBoundary(const BoundaryNode *boundary) const;
        bool hasPendingBoundary(const BoundaryNode *boundary) const;
        BoundaryNode *firstPendingBoundary() const;
        BoundaryNode *rootBoundaryFor(Node *rootNode) const;
        BoundaryNode *topMostRequestedBoundary(BoundaryNode *boundary) const;
        bool isBoundaryUpdateRoot(BoundaryNode *boundary) const;
        BoundaryNode *firstPendingUpdateRoot() const;
        BoundaryNode *primaryUpdateRootFor(Node *rootNode) const;
        BoundaryNode *nextPendingUpdateRoot(BoundaryNode *afterRoot) const;
        SceneUpdateRequestSnapshot buildRefreshRequestSnapshot(Node *rootNode) const;
        SceneUpdateApplySnapshot buildApplySnapshot(const Scene *scene) const;
        void finalizeUpdateSnapshot(SceneUpdateSnapshot &snapshot,
                                    const Scene *scene) const;
        SceneUpdateSnapshot buildUpdateSnapshot(Node *rootNode,
                                               const Scene *scene) const;
        PlatformApplyPlan buildPlatformApplyPlan(const SceneUpdateSnapshot &snapshot) const;
        PlatformApplyPlan executeApplyPlan(Node *rootNode,
                                          IPlatformController *platformController,
                                          const SceneUpdateSnapshot &snapshot,
                                          NodeDirtyFlags globalDirtyFlags,
                                          bool fullRebuild) const;
        void applyPlatformApplyPlan(Node *rootNode,
                                    IPlatformController *platformController,
                                    const PlatformApplyPlan &plan,
                                    NodeDirtyFlags globalDirtyFlags,
                                    bool fullRebuild) const;
        void applyPendingBoundaryUpdate(Node *rootNode,
                                        BoundaryNode *root,
                                        const PlatformApplyPlan &plan) const;
        void applyPendingBoundaryUpdates(Node *rootNode,
                                         const PlatformApplyPlan &plan) const;
        bool shouldApplyGlobalChange(IPlatformController *platformController,
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
          explicit PendingUpdateRootAnalysis(const SceneDirector *director);
          ~PendingUpdateRootAnalysis();
          bool hasEquivalentDescendant(BoundaryNode *root) const;
          bool hasSeenRoot(BoundaryNode *root) const;
          void recordSeenRoot(BoundaryNode *root);
          bool shouldSkip(BoundaryNode *boundary, BoundaryNode *root) const;

          struct SeenRoot
          {
            SeenRoot() : root(0), next(0) {}

            BoundaryNode *root;
            SeenRoot *next;
          };

          const SceneDirector *director;
          SeenRoot *seenHead;
          SeenRoot *seenTail;

        private:
          PendingUpdateRootAnalysis(const PendingUpdateRootAnalysis &);
          PendingUpdateRootAnalysis &operator=(const PendingUpdateRootAnalysis &);
        };

        struct PendingUpdateRootCursor
        {
          explicit PendingUpdateRootCursor(const SceneDirector *director);
          BoundaryNode *next();

          const SceneDirector *director;
          SceneProjectionTransaction::ConstIterator iterator;
          PendingUpdateRootAnalysis analysis;
        };

        BoundaryUpdateRequest normalizeBoundaryUpdateRequest(BoundaryNode *boundary,
                                                            NodeDirtyFlags flags,
                                                            bool flushImmediately) const;
        void applyBoundaryUpdateRequest(const BoundaryUpdateRequest &request) const;
        NodeDirtyFlags aggregateDirtyFlags() const;
        NodeDirtyFlags requestedDirtyFlags() const;
        NodeDirtyFlags effectiveRequestedDirtyFlags() const;
        bool hasRequestedInput() const;
        bool requestedFullRebuild() const;

        Scene *scene_;
        SceneUpdateTransaction updateTransaction_;

        friend class ::loka::dsl::testing::SceneTestAccess;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_SCENEDIRECTOR_HPP
