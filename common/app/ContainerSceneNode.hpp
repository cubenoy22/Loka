#ifndef DECLARA_CONTAINERSCENENODE_HPP
#define DECLARA_CONTAINERSCENENODE_HPP

#include "core/SceneNode.hpp"
#include "core/SceneNodeGroup.hpp"
#include "core/util/SceneNodeAttachScope.hpp"
#include "core/SceneNodeFactory.hpp"
#include <vector>

// --- ContainerSceneNode: 子ノード管理の共通基底クラス ---
class ContainerSceneNode : public SceneNode
{
public:
  ContainerSceneNode() {}
  virtual ~ContainerSceneNode() {}

  virtual ContainerSceneNode *addChild(SceneNode *child)
  {
    if (child)
    {
      // Remove from previous parent group if needed
      if (child->getParentGroup() && child->getParentGroup() != reinterpret_cast<SceneNodeGroup *>(this))
      {
        child->getParentGroup()->remove(child);
      }
      // Remove from previous parent in this container (avoid duplicates)
      for (std::vector<SceneNode *>::iterator it = children_.begin(); it != children_.end(); ++it)
      {
        if (*it == child)
        {
          children_.erase(it);
          break;
        }
      }
      children_.push_back(child);
      child->setParentGroup(reinterpret_cast<SceneNodeGroup *>(this)); // treat as group for parent pointer
    }
    return this;
  }

  inline ContainerSceneNode &operator<<(SceneNode *child)
  {
    addChild(child);
    return *this;
  }

  const std::vector<SceneNode *> &children() const { return children_; }

protected:
  std::vector<SceneNode *> children_;
};

#endif // DECLARA_CONTAINERSCENENODE_HPP
