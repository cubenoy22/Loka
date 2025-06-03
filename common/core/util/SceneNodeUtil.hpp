#ifndef DECLARA_CORE_UTIL_SCENENODEUTIL_HPP
#define DECLARA_CORE_UTIL_SCENENODEUTIL_HPP

#include "core/util/SceneNodeAttachScope.hpp"
#include "SceneNodeAttachScope.hpp"

class SceneNode;
class SceneNodeGroup;

// 汎用DSL生成関数（NodeDefinitionインスタンスを引数1つで渡す方式）
template <typename DefinitionT>

inline SceneNode *Node(const DefinitionT &def, bool autoAttach = true)
{
  SceneNode *node = SceneNodeFactory::instance()->getOrCreate(&def.props);
  if (autoAttach)
  {
#ifdef TEST_BUILD
    printf("[Node] about to autoAttach for %p\n", node);
#endif
    SceneNodeAttachScope::autoAttach(node);
  }
  return node;
}

template <typename DefT>
inline typename DefT::NodeType *NodeAs(const DefT &def, bool autoAttach = true)
{
  return static_cast<typename DefT::NodeType *>(Node(def, autoAttach));
}

// テストコードの形式と合わせたインターフェース
inline void AttachScope(SceneNodeGroup *group)
{
  AttachScopeGuard guard;
  // スコープを開始する際、前のターゲットをスタックから削除しない
  attachTargetStack.push_back({AttachTarget::Group, group});
}

inline void AttachScope(LayoutSceneNode *layout)
{
  AttachScopeGuard guard;
  // スコープを開始する際、前のターゲットをスタックから削除しない
  attachTargetStack.push_back({AttachTarget::Layout, layout});
}

// 明示的にスコープを閉じる関数 - ブロックスコープが終わったら呼び出す
inline void EndScope()
{
  // AttachScopeGuardのデストラクタでスコープが閉じるようになったが、
  // 明示的に閉じる場合はこのメソッドを使用する
  SceneNodeAttachScope::endScope();
}

// RAII版スコープクラス - ブロックで明示的に管理する場合
class ScopedAttach
{
public:
  ScopedAttach(AttachTarget::Type type, void *ptr)
  {
#ifdef TEST_BUILD
    printf("[ScopedAttach] push %p (type=%d)\n", ptr, (int)type);
#endif
    attachTargetStack.push_back({type, ptr});
  }
  ~ScopedAttach()
  {
#ifdef TEST_BUILD
    printf("[ScopedAttach] pop stack (size=%zu)\n", attachTargetStack.size());
#endif
    if (!attachTargetStack.empty())
    {
      attachTargetStack.pop_back();
    }
  }
};

// 必要に応じて他のノード型も追加可能

#endif // DECLARA_CORE_UTIL_SCENENODEUTIL_HPP
