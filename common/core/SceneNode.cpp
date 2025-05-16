#include "core/SceneNode.hpp"
#include "core/SceneNodeGroup.hpp"

SceneNode::NodeReuseCategory SceneNode::getReuseCategory() const {
    if (reuseCategory_ == SceneNode::Reuse_FollowParent && parentGroup_)
        return parentGroup_->getReuseCategory();
    return reuseCategory_;
}

SceneNode::NodeReuseHeuristic SceneNode::getReuseHeuristic() const {
    if (reuseHeuristic_ == SceneNode::ReuseHeuristic_FollowParent && parentGroup_)
        return parentGroup_->getReuseHeuristic();
    return reuseHeuristic_;
}
