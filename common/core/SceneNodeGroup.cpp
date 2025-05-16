#include "core/SceneNodeGroup.hpp"

// NodeGroupScopeのグローバルstatic変数の定義（多重定義防止）
SceneNodeGroup *NodeGroupScope::currentGroup_ = 0;
