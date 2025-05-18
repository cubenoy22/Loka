#include "core/SceneNodeGroup.hpp"
#include "core/util/SceneNodeAttachScope.hpp"

SceneNodeGroup::SceneNodeGroup(PoolType *pool, NodeReuseCategory cat, NodeReuseHeuristic heur)
    : SceneNode(), nodePool_(pool ? pool : new SceneNodeDefaultPool<SceneNode>()),
      boundState_(0), reuseCategory_(cat), reuseHeuristic_(heur), parentGroup_(0)
{
  SceneNodeAttachScope::autoAttach(static_cast<SceneNode *>(this));
}

SceneNodeGroup::SceneNodeGroup(const std::vector<StateBase *> &recomposeDeps, PoolType *pool, NodeReuseCategory cat, NodeReuseHeuristic heur)
    : SceneNode(), nodePool_(pool ? pool : new SceneNodeDefaultPool<SceneNode>()),
      boundState_(0), reuseCategory_(cat), reuseHeuristic_(heur), parentGroup_(0)
{
  for (size_t i = 0; i < recomposeDeps.size(); ++i)
  {
    if (recomposeDeps[i])
      bindDefer(recomposeDeps[i]);
  }
  SceneNodeAttachScope::autoAttach(static_cast<SceneNode *>(this));
}

// NodeGroupScopeのグローバルstatic変数の定義（多重定義防止）
SceneNodeGroup *NodeGroupScope::currentGroup_ = 0;
