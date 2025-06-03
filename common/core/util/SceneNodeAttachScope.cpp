#include "core/SceneNode.hpp"
#include "core/util/SceneNodeAttachScope.hpp"
#include "core/SceneNodeGroup.hpp"
#include "app/LayoutSceneNode.hpp"

std::vector<AttachTarget> attachTargetStack;
int AttachScopeGuard::scopeDepth = 0;
AttachTarget AttachScopeGuard::prevTarget = {AttachTarget::Group, nullptr};
void SceneNodeAttachScope::autoAttach(SceneNode *node)
{
  AttachTarget t = current();
#ifdef TEST_BUILD
  printf("[autoAttach] node=%p type=%s ", node, typeid(*node).name());
  if (t.ptr)
    printf("target=%d ptr=%p\n", (int)t.type, t.ptr);
  else
    printf("target=NULL ptr=NULL\n");
#endif

  if (t.ptr)
  {
    if (t.type == AttachTarget::Group)
    {
      static_cast<SceneNodeGroup *>(t.ptr)->add(node);
    }
    else if (t.type == AttachTarget::Layout)
    {
      static_cast<LayoutSceneNode *>(t.ptr)->addChild(node);
    }
  }
}

// 明示的にスコープを閉じる関数
void SceneNodeAttachScope::endScope()
{
  if (!attachTargetStack.empty())
  {
    // 現在のスコープのターゲットを削除
    attachTargetStack.pop_back();

    // スコープ深度を減らす（AttachScopeGuardと合わせて管理）
    if (AttachScopeGuard::scopeDepth > 0)
    {
      AttachScopeGuard::scopeDepth--;
    }
  }
}
