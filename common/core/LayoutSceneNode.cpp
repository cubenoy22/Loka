#include "core/LayoutSceneNode.hpp"
#include "core/util/SceneNodeAttachScope.hpp"

LayoutSceneNode::LayoutSceneNode(Box::Direction dir)
    : SceneNode(), box_(dir)
{
  SceneNodeAttachScope::autoAttach(this);
}
