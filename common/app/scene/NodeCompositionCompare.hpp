#ifndef LOKA_CORE2_SCENE_NODECOMPOSITIONCOMPARE_HPP
#define LOKA_CORE2_SCENE_NODECOMPOSITIONCOMPARE_HPP

#include <vector>
#include "app/scene/NodeComposition.hpp"
#include "app/scene/NodeCompositionDiff.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      namespace detail
      {
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

      inline bool buildNodeCompositionDiffByTag(NodeComposition &previous,
                                                NodeComposition &current,
                                                NodeCompositionDiff &out)
      {
        out.clear();

        NodeDefinitionBase *previousRoot = previous.root();
        NodeDefinitionBase *currentRoot = current.root();
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
        if (!previousNestable || !currentNestable)
        {
          return false;
        }

        std::vector<NodeDefinitionBase *> previousChildren;
        std::vector<NodeDefinitionBase *> currentChildren;
        if (!detail::collectTaggedChildren(previousNestable, previousChildren))
        {
          return false;
        }
        if (!detail::collectTaggedChildren(currentNestable, currentChildren))
        {
          return false;
        }

        for (size_t i = 0; i < currentChildren.size(); ++i)
        {
          NodeDefinitionBase *currentChild = currentChildren[i];
          int previousIndex = detail::indexOfTag(previousChildren, currentChild->nodeTag());
          if (previousIndex >= 0)
          {
            out.addEntry(currentChild->nodeTag(), static_cast<int>(i), NodeCompositionDiff::ACTION_RETAIN, previousIndex, static_cast<int>(i));
          }
          else
          {
            out.addEntry(currentChild->nodeTag(), static_cast<int>(i), NodeCompositionDiff::ACTION_REPLACE, -1, static_cast<int>(i));
          }
        }

        for (size_t i = 0; i < previousChildren.size(); ++i)
        {
          NodeDefinitionBase *previousChild = previousChildren[i];
          int currentIndex = detail::indexOfTag(currentChildren, previousChild->nodeTag());
          if (currentIndex < 0)
          {
            out.addEntry(previousChild->nodeTag(), static_cast<int>(i), NodeCompositionDiff::ACTION_RETIRE, static_cast<int>(i), -1);
          }
        }

        out.valid = true;
        out.fullRebuild = false;
        return true;
      }
    }
  }
}

#endif // LOKA_CORE2_SCENE_NODECOMPOSITIONCOMPARE_HPP
