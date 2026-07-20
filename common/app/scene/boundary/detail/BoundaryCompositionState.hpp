#ifndef LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARYCOMPOSITIONSTATE_HPP
#define LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARYCOMPOSITIONSTATE_HPP

#include <vector>
#include "app/scene/Node.hpp"
#include "app/scene/composition/NodeCompositionCompare.hpp"
#include "app/scene/composition/NodeCompositionDiff.hpp"
#include "app/scene/composition/NodeCompositionSnapshot.hpp"
#include "app/scene/boundary/BoundaryStateTypes.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      struct BoundaryLocalRebuildPlanEntry
      {
        enum Action
        {
          ACTION_RETAIN = 0,
          ACTION_ATTACH = 1,
          ACTION_REPLACE = 2,
          ACTION_RETIRE = 3
        };

        BoundaryLocalRebuildPlanEntry()
            : node(0),
              previousNode(0),
              definition(0),
              action(ACTION_RETAIN),
              tag(NODE_TAG_NONE)
        {
        }

        static BoundaryLocalRebuildPlanEntry retain(Node *nodeValue,
                                                    NodeDefinitionBase *definitionValue,
                                                    NodeTag tagValue)
        {
          BoundaryLocalRebuildPlanEntry entry;
          entry.node = nodeValue;
          entry.definition = definitionValue;
          entry.tag = tagValue;
          return entry;
        }

        static BoundaryLocalRebuildPlanEntry attach(Node *nodeValue,
                                                    NodeDefinitionBase *definitionValue,
                                                    NodeTag tagValue)
        {
          BoundaryLocalRebuildPlanEntry entry;
          entry.node = nodeValue;
          entry.definition = definitionValue;
          entry.action = ACTION_ATTACH;
          entry.tag = tagValue;
          return entry;
        }

        static BoundaryLocalRebuildPlanEntry attach(Node *nodeValue, NodeTag tagValue)
        {
          return attach(nodeValue, 0, tagValue);
        }

        static BoundaryLocalRebuildPlanEntry replace(Node *nodeValue,
                                                     Node *previousNodeValue,
                                                     NodeDefinitionBase *definitionValue,
                                                     NodeTag tagValue)
        {
          BoundaryLocalRebuildPlanEntry entry;
          entry.node = nodeValue;
          entry.previousNode = previousNodeValue;
          entry.definition = definitionValue;
          entry.action = ACTION_REPLACE;
          entry.tag = tagValue;
          return entry;
        }

        static BoundaryLocalRebuildPlanEntry replace(Node *nodeValue,
                                                     Node *previousNodeValue,
                                                     NodeTag tagValue)
        {
          return replace(nodeValue, previousNodeValue, 0, tagValue);
        }

        static BoundaryLocalRebuildPlanEntry retire(Node *nodeValue, NodeTag tagValue)
        {
          BoundaryLocalRebuildPlanEntry entry;
          entry.node = nodeValue;
          entry.action = ACTION_RETIRE;
          entry.tag = tagValue;
          return entry;
        }

        bool keepsLiveNode() const
        {
          return this->action != ACTION_RETIRE;
        }

        bool requiresAttachCompose() const
        {
          return this->action == ACTION_ATTACH || this->action == ACTION_REPLACE;
        }

        Node *detachedNode() const
        {
          if (this->action == ACTION_REPLACE)
          {
            return this->previousNode;
          }
          if (this->action == ACTION_RETIRE)
          {
            return this->node;
          }
          return 0;
        }

        Node *node;
        Node *previousNode;
        /** Borrowed from the current compose arena for this apply pass. */
        NodeDefinitionBase *definition;
        Action action;
        NodeTag tag;
      };

      struct BoundaryLocalRebuildPlan
      {
        void reserve(size_t count)
        {
          entries.reserve(count);
        }

        void clear()
        {
          entries.clear();
        }

        std::vector<BoundaryLocalRebuildPlanEntry> entries;
      };

      struct BoundaryCompositionState
      {
        BoundaryCompositionState()
            : result(),
              previousSnapshot(),
              currentSnapshot(),
              diff()
        {
        }

        void clearResult()
        {
          result.clear();
        }

        void beginCompose(ComposeEvent event, NodeDirtyFlags dirtyFlags)
        {
          result.event = event;
          result.dirtyFlagsSeen = dirtyFlags;
          result.composed = false;
          result.preservedNativeContexts = false;
          result.allocationFailed = false;
        }

        void completeCompose(bool preservedNativeContexts)
        {
          result.composed = true;
          result.preservedNativeContexts = preservedNativeContexts;
        }

        void noteAllocationFailure()
        {
          result.allocationFailed = true;
        }

        bool allocationFailedValue() const
        {
          return result.allocationFailed;
        }

        /** White-flag terminal half A (#132 ruling 3): the compose stays a
            failure — composed remains false and nothing may treat the local
            diff as applicable. allocationFailed stays visible. */
        void failCompose()
        {
          result.composed = false;
          result.preservedNativeContexts = false;
        }

        /** White-flag terminal half B — the #70 mechanism: a failed compose
            must not leave snapshots that could seed a stale retain diff. An
            empty previous snapshot forces the next compose onto the
            clean-slate rebuild path. */
        void invalidateSnapshots()
        {
          previousSnapshot.clear();
          currentSnapshot.clear();
          diff.clear();
        }

        BoundaryComposeResult &composeResult()
        {
          return result;
        }

        const BoundaryComposeResult &composeResult() const
        {
          return result;
        }

        void captureCurrentSnapshot(const NodeComposition &composition)
        {
          currentSnapshot.capture(composition);
        }

        void rebuildLocalCompositionDiff()
        {
          this->diff.clear();
          if (!buildNodeCompositionSnapshotDiffByTag(this->previousSnapshot, this->currentSnapshot, this->diff))
          {
            this->diff.clear();
          }
        }

        void promoteCurrentSnapshot()
        {
          previousSnapshot = currentSnapshot;
          currentSnapshot.clear();
        }

        NodeDefinitionBase *findCurrentDefinitionByTag(NodeTag tag) const
        {
          if (tag == NODE_TAG_NONE)
          {
            return 0;
          }
          const INestableDefinition *root = currentRootNestableDefinition();
          if (!root)
          {
            return 0;
          }
          NodeDefinitionBase *child = root->childrenHead();
          while (child)
          {
            if (child->nodeTag() == tag)
            {
              return child;
            }
            child = child->nextInComposition;
          }
          return 0;
        }

        INestableDefinition *currentRootNestableDefinition() const
        {
          return currentSnapshot.root() ? currentSnapshot.root()->asNestableDefinition() : 0;
        }

        const NodeCompositionDiff *localCompositionDiff() const
        {
          return this->diff.valid ? &this->diff : 0;
        }

        bool hasCompositionDiffState() const
        {
          return !this->previousSnapshot.empty() || !this->currentSnapshot.empty() || !this->diff.empty();
        }

        bool canApplyLocalCompositionDiff() const
        {
          const NodeCompositionDiff *diff = localCompositionDiff();
          return diff != 0 && !diff->fullRebuild && !diff->empty() && !diff->hasIncompatibleRetain();
        }

        bool canPreserveNativeContexts() const
        {
          const NodeCompositionDiff *diff = localCompositionDiff();
          return diff != 0 && !diff->fullRebuild && !diff->hasIncompatibleRetain();
        }

        NodeCompositionSnapshot &previousCompositionSnapshot()
        {
          return previousSnapshot;
        }

        const NodeCompositionSnapshot &previousCompositionSnapshot() const
        {
          return previousSnapshot;
        }

        NodeCompositionSnapshot &currentCompositionSnapshot()
        {
          return currentSnapshot;
        }

        const NodeCompositionSnapshot &currentCompositionSnapshot() const
        {
          return currentSnapshot;
        }

        BoundaryComposeResult result;
        NodeCompositionSnapshot previousSnapshot;
        NodeCompositionSnapshot currentSnapshot;
        NodeCompositionDiff diff;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif
