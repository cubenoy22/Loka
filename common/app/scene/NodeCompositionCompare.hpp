#ifndef LOKA_CORE2_SCENE_NODECOMPOSITIONCOMPARE_HPP
#define LOKA_CORE2_SCENE_NODECOMPOSITIONCOMPARE_HPP

#include <vector>
#include "app/scene/NodeCompositionDiff.hpp"
#include "app/scene/NodeCompositionSnapshot.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      namespace detail
      {
        inline bool buildSingleAnonymousChildDiff(INestableDefinition *previousParent,
                                                  INestableDefinition *currentParent,
                                                  NodeCompositionDiff &out)
        {
          if (!previousParent || !currentParent)
          {
            return false;
          }
          if (previousParent->childrenCount() != 1 || currentParent->childrenCount() != 1)
          {
            return false;
          }
          NodeDefinitionBase *previousChild = previousParent->childrenHead();
          NodeDefinitionBase *currentChild = currentParent->childrenHead();
          if (!previousChild || !currentChild)
          {
            return false;
          }
          if (previousChild->nodeTag() != NODE_TAG_NONE || currentChild->nodeTag() != NODE_TAG_NONE)
          {
            return false;
          }

          const scene::PropsBase *previousProps = previousChild->propsBase();
          const scene::PropsBase *currentProps = currentChild->propsBase();
          const bool compatibleType = previousProps && currentProps &&
                                      previousProps->propsTypeId() == currentProps->propsTypeId();
          const bool equivalentProps = compatibleType && previousChild->hasEquivalentProps(*currentChild);
          out.addEntry(NODE_TAG_NONE,
                       0,
                       compatibleType ? NodeCompositionDiff::ACTION_RETAIN : NodeCompositionDiff::ACTION_REPLACE,
                       compatibleType,
                       equivalentProps,
                       0,
                       0);
          out.valid = true;
          out.fullRebuild = false;
          return true;
        }

        inline bool collectTaggedChildren(INestableDefinition *parent, std::vector<NodeDefinitionBase *> &out)
        {
          out.clear();
          if (!parent)
          {
            return true;
          }

          NodeDefinitionBase *child = parent->childrenHead();
          while (child)
          {
            if (child->nodeTag() == NODE_TAG_NONE)
            {
              return false;
            }
            for (size_t i = 0; i < out.size(); ++i)
            {
              if (out[i] && out[i]->nodeTag() == child->nodeTag())
              {
                return false;
              }
            }
            out.push_back(child);
            child = child->nextInComposition;
          }
          return true;
        }

        inline int indexOfTag(const std::vector<NodeDefinitionBase *> &children, NodeTag tag)
        {
          for (size_t i = 0; i < children.size(); ++i)
          {
            if (children[i] && children[i]->nodeTag() == tag)
            {
              return static_cast<int>(i);
            }
          }
          return -1;
        }
      } // namespace detail

      inline bool buildNodeCompositionSnapshotRootDiffByTag(NodeDefinitionBase *previousRoot,
                                                            NodeDefinitionBase *currentRoot,
                                                            NodeCompositionDiff &out)
      {
        out.clear();

        if (!previousRoot && !currentRoot)
        {
          out.valid = true;
          out.fullRebuild = false;
          return true;
        }
        if (!previousRoot || !currentRoot)
        {
          return false;
        }

        INestableDefinition *previousNestable = previousRoot->asNestableDefinition();
        INestableDefinition *currentNestable = currentRoot->asNestableDefinition();
        if (!previousNestable && !currentNestable)
        {
          const scene::PropsBase *previousProps = previousRoot->propsBase();
          const scene::PropsBase *currentProps = currentRoot->propsBase();
          const bool compatibleType = previousProps && currentProps &&
                                      previousProps->propsTypeId() == currentProps->propsTypeId();
          const bool equivalentProps = compatibleType && previousRoot->hasEquivalentProps(*currentRoot);
          NodeTag rootTag = currentRoot->nodeTag();
          if (rootTag == NODE_TAG_NONE)
          {
            rootTag = previousRoot->nodeTag();
          }
          out.addEntry(rootTag,
                       0,
                       compatibleType ? NodeCompositionDiff::ACTION_RETAIN : NodeCompositionDiff::ACTION_REPLACE,
                       compatibleType,
                       equivalentProps,
                       0,
                       0);
          out.valid = true;
          out.fullRebuild = false;
          return true;
        }
        if (!previousNestable || !currentNestable)
        {
          return false;
        }

        std::vector<NodeDefinitionBase *> previousChildren;
        std::vector<NodeDefinitionBase *> currentChildren;
        if (!detail::collectTaggedChildren(previousNestable, previousChildren))
        {
          return detail::buildSingleAnonymousChildDiff(previousNestable, currentNestable, out);
        }
        if (!detail::collectTaggedChildren(currentNestable, currentChildren))
        {
          return detail::buildSingleAnonymousChildDiff(previousNestable, currentNestable, out);
        }

        for (size_t i = 0; i < currentChildren.size(); ++i)
        {
          NodeDefinitionBase *currentChild = currentChildren[i];
          int previousIndex = detail::indexOfTag(previousChildren, currentChild->nodeTag());
          if (previousIndex >= 0)
          {
            NodeDefinitionBase *previousChild = previousChildren[previousIndex];
            const scene::PropsBase *previousProps = previousChild ? previousChild->propsBase() : 0;
            const scene::PropsBase *currentProps = currentChild ? currentChild->propsBase() : 0;
            bool compatibleType = previousProps && currentProps &&
                                  previousProps->propsTypeId() == currentProps->propsTypeId();
            bool equivalentProps = compatibleType && previousChild->hasEquivalentProps(*currentChild);
            out.addEntry(currentChild->nodeTag(),
                         static_cast<int>(i),
                         compatibleType ? NodeCompositionDiff::ACTION_RETAIN : NodeCompositionDiff::ACTION_REPLACE,
                         compatibleType,
                         equivalentProps,
                         previousIndex,
                         static_cast<int>(i));
          }
          else
          {
            out.addEntry(currentChild->nodeTag(),
                         static_cast<int>(i),
                         NodeCompositionDiff::ACTION_REPLACE,
                         false,
                         false,
                         -1,
                         static_cast<int>(i));
          }
        }

        for (size_t i = 0; i < previousChildren.size(); ++i)
        {
          NodeDefinitionBase *previousChild = previousChildren[i];
          int currentIndex = detail::indexOfTag(currentChildren, previousChild->nodeTag());
          if (currentIndex < 0)
          {
            out.addEntry(previousChild->nodeTag(),
                         static_cast<int>(i),
                         NodeCompositionDiff::ACTION_RETIRE,
                         false,
                         false,
                         static_cast<int>(i),
                         -1);
          }
        }

        out.valid = true;
        out.fullRebuild = false;
        return true;
      }

      inline bool buildNodeCompositionSnapshotDiffByTag(const NodeCompositionSnapshot &previous,
                                                        const NodeCompositionSnapshot &current,
                                                        NodeCompositionDiff &out)
      {
        return buildNodeCompositionSnapshotRootDiffByTag(previous.root(), current.root(), out);
      }
    }
  }
}

#endif // LOKA_CORE2_SCENE_NODECOMPOSITIONCOMPARE_HPP
