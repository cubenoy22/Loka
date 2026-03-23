#ifndef LOKA_CORE2_SCENE_NODE_BOUNDARYCOMPOSITIONSTATE_HPP
#define LOKA_CORE2_SCENE_NODE_BOUNDARYCOMPOSITIONSTATE_HPP

#include <vector>
#include "app/scene/Node.hpp"
#include "app/scene/NodeCompositionSnapshot.hpp"
#include "app/scene/NodeCompositionTransaction.hpp"
#include "app/scene/node/BoundaryStateTypes.hpp"

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
              action(ACTION_RETAIN),
              tag(NODE_TAG_NONE)
        {
        }

        Node *node;
        Node *previousNode;
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
              transaction()
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
        }

        void completeCompose(bool preservedNativeContexts)
        {
          result.composed = true;
          result.preservedNativeContexts = preservedNativeContexts;
        }

        void captureCurrentSnapshot(const NodeComposition &composition)
        {
          currentSnapshot.capture(composition);
        }

        void rebuildTransaction()
        {
          transaction.beginSnapshots(&previousSnapshot, &currentSnapshot);
          if (!transaction.buildDiffByTag())
          {
            transaction.diff().clear();
          }
        }

        void promoteCurrentSnapshot()
        {
          previousSnapshot = currentSnapshot;
          currentSnapshot.clear();
        }

        NodeDefinitionBase *findCurrentDefinitionByTag(NodeTag tag) const
        {
          const INestableDefinition *root = currentSnapshot.root()
                                                ? currentSnapshot.root()->asNestableDefinition()
                                                : 0;
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

        BoundaryComposeResult result;
        NodeCompositionSnapshot previousSnapshot;
        NodeCompositionSnapshot currentSnapshot;
        NodeCompositionTransaction transaction;
      };
    }
  }
}

#endif
