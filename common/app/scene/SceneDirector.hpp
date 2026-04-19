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
          struct RequestedSceneInput
          {
            RequestedSceneInput()
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

            bool hasPending() const
            {
              return dirtyFlags != NODE_DIRTY_NONE;
            }

            NodeDirtyFlags dirtyFlags;
            bool fullRebuild;
          };

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
                pendingBoundaries(),
                requestedInput()
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

          void enqueueSceneRequest(NodeDirtyFlags flags)
          {
            requestedInput.include(flags);
          }

          NodeDirtyFlags pendingRequestedDirtyFlags() const
          {
            return requestedInput.dirtyFlags;
          }

          NodeDirtyFlags effectiveRequestedDirtyFlags() const
          {
            return requestedInput.effectiveDirtyFlags();
          }

          bool hasPendingRequestedInput() const
          {
            return requestedInput.hasPending();
          }

          bool pendingRequestedFullRebuild() const
          {
            return requestedInput.fullRebuild;
          }

          NodeDirtyFlags pendingDirtyFlagsForBoundary(const BoundaryNode *boundary) const
          {
            if (!boundary)
            {
              return NODE_DIRTY_NONE;
            }
            const SceneProjectionTransaction::TargetEntry *entry = projectionTransaction().targetsHead();
            while (entry)
            {
              if (static_cast<const void *>(entry->node) == static_cast<const void *>(boundary))
              {
                return entry->dirtyFlags;
              }
              entry = entry->next;
            }
            return NODE_DIRTY_NONE;
          }

          void enqueueBoundaryUpdate(const BoundaryUpdateRequest &request);
          void enqueuePendingBoundary(BoundaryNode *boundary);
          void clearPendingState();

          SceneUpdateRequestSnapshot buildRequestSnapshot(Node *rootNode,
                                                         BoundaryNode *firstPendingRoot) const
          {
            SceneUpdateRequestSnapshot snapshot;
            if (!hasPendingWave())
            {
              return snapshot;
            }
            const NodeDirtyFlags transactionDirtyFlags = aggregateDirtyFlags();
            snapshot.setRequestedInput(effectiveRequestedDirtyFlags(), pendingRequestedFullRebuild(), rootNode ? rootNode->asBoundary() : 0);
            snapshot.setTransactionDirtyFlags(transactionDirtyFlags);
            snapshot.deriveEffectiveFullRebuild();
            snapshot.setFirstPendingRoot(firstPendingRoot);
            return snapshot;
          }

          void clear()
          {
            pendingWave.clear();
            pendingBoundaries.clear();
            requestedInput.clear();
          }

          PendingProjectionWave pendingWave;
          PendingBoundaryQueue pendingBoundaries;
          RequestedSceneInput requestedInput;
        };

        SceneDirector();

        void attach(Scene *scene);
        void detach();

        void registerBoundaryUpdate(const BoundaryUpdateRequest &request);
        void requestBoundaryUpdate(BoundaryNode *boundary, NodeDirtyFlags flags, bool flushImmediately);

        const SceneProjectionTransaction &projectionTransaction() const;
        NodeDirtyFlags aggregateDirtyFlags() const;
        NodeDirtyFlags pendingRequestedDirtyFlags() const;
        NodeDirtyFlags effectiveRequestedDirtyFlags() const;
        bool hasPendingRequestedInput() const;
        bool pendingRequestedFullRebuild() const;
        NodeDirtyFlags pendingDirtyFlagsForBoundary(const BoundaryNode *boundary) const;
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
