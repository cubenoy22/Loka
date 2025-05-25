#ifndef DECLARA_SCENENODEATTACHSCOPE_HPP
#define DECLARA_SCENENODEATTACHSCOPE_HPP

#include <vector>

// 前方宣言で循環依存を回避
class SceneNodeGroup;
class LayoutSceneNode;
class SceneNode;

struct AttachTarget
{
  enum Type
  {
    Group,
    Layout
  } type;
  void *ptr;
};
extern std::vector<AttachTarget> attachTargetStack;

// スコープ深度を管理するクラス
class AttachScopeGuard
{
private:
  static int scopeDepth;
  static AttachTarget prevTarget;

public:
  friend class SceneNodeAttachScope; // SceneNodeAttachScopeからscopeDepthにアクセスできるようにする
  AttachScopeGuard()
  {
    scopeDepth++;
    // 現在のターゲットを保存しておく
    if (!attachTargetStack.empty())
      prevTarget = attachTargetStack.back();
  }
  ~AttachScopeGuard()
  {
    scopeDepth--;
    if (scopeDepth <= 0)
    {
      // スコープの最後で自動的にattachTargetStackからポップ
      if (!attachTargetStack.empty())
        attachTargetStack.pop_back();
    }
    else if (!attachTargetStack.empty() && scopeDepth > 0)
    {
      // 内部スコープ終了時に親スコープのターゲットに戻す
      attachTargetStack.pop_back();
      // 親スコープのターゲットを復元
      if (prevTarget.ptr != nullptr)
      {
        attachTargetStack.push_back(prevTarget);
      }
    }
  }
};

class SceneNodeAttachScope
{
public:
  SceneNodeAttachScope(AttachTarget::Type type, void *ptr)
  {
    attachTargetStack.push_back({type, ptr});
  }
  ~SceneNodeAttachScope()
  {
    if (!attachTargetStack.empty())
      attachTargetStack.pop_back();
  }
  static AttachTarget current()
  {
    return attachTargetStack.empty() ? AttachTarget{AttachTarget::Group, nullptr} : attachTargetStack.back();
  }
  static void autoAttach(SceneNode *node);
  static void endScope(); // スコープを閉じるための静的メソッド
};

#endif // DECLARA_SCENENODEATTACHSCOPE_HPP
