#include "app/ContainerSceneNode.hpp"
#include "app/LayoutSceneNode.hpp"
#include "core/util/SceneNodeAttachScope.hpp"

LayoutSceneNode::LayoutSceneNode(Box::Direction dir)
    : ContainerSceneNode(), box_(dir)
{
    SceneNodeAttachScope::autoAttach(this);
}
