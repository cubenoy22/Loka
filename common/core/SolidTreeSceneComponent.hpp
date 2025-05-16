#ifndef DECLARA_SOLIDTREESCENECOMPONENT_HPP
#define DECLARA_SOLIDTREESCENECOMPONENT_HPP

#include "core/TreedSceneComponent.hpp"
#include "core/StateTracker.hpp"
#include "core/SceneNode.hpp"
#include <vector>
#include <functional>

// --- SolidTreeSceneComponent: Solid.js風ローカルリアクティブUIツリー ---
class SolidTreeSceneComponent : public TreedSceneComponent
{
public:
  typedef void (*ComposeFn)(SolidTreeSceneComponent &);
  // --- SceneNodeController型定義（今後の拡張用） ---
  typedef SceneNodeController<SceneNodeAllocator<SceneNode>, SceneNode> ControllerType;

  SolidTreeSceneComponent()
      : tracker_(), dirty_(true), controller_(nullptr) {}

  // 新規: compose関数を受け取るコンストラクタ
  SolidTreeSceneComponent(ComposeFn fn)
      : tracker_(), dirty_(true), controller_(nullptr)
  {
    if (fn)
      fn(*this);
  }
  // C++11以降ならstd::functionやラムダも可。C++98互換のため関数ポインタ優先。

  virtual ~SolidTreeSceneComponent()
  {
    delete controller_;
  }

  // Solid.js風: ローカルStateTrackerで差分検知
  void markDirty() { dirty_ = true; }
  bool isDirty() const { return dirty_; }

  // 再描画・差分検知のエントリポイント
  virtual void update()
  {
    if (!dirty_)
      return;
    // 子も再帰的にupdate
    for (size_t i = 0; i < children.size(); ++i)
    {
      TreedSceneComponent *child = dynamic_cast<TreedSceneComponent *>(children[i]);
      if (child)
        child->update();
    }
    dirty_ = false;
  }

  // StateTracker管理API（必要に応じて拡張）
  PushStateTracker &tracker() { return tracker_; }

protected:
  PushStateTracker tracker_;
  bool dirty_;
  // --- SceneNodeControllerメンバ（今後の拡張用） ---
  ControllerType *controller_;
};

#endif // DECLARA_SOLIDTREESCENECOMPONENT_HPP
