#ifndef DECLARA_SCENE_HPP
#define DECLARA_SCENE_HPP

#include "app/Button.hpp"
#include "app/Text.hpp"
#include "app/TextInput.hpp"
#include <string>
#include <vector>
#include "core/SceneComponent.hpp"
#include "core/SceneContext.hpp"
#include "core/SceneNodeGroup.hpp"

class PlatformContext;

class SceneHost
{
};

class Scene
{
public:
  enum ScenePhase
  {
    ATTACHED,
    DETACHED
  };

  Scene(SceneHost *host)
      : rootGroup_(new SceneNodeGroup()), currentPhase(DETACHED), context_(nullptr)
  {
    // 直接SceneNode/SceneNodeGroupを構築する設計に統一
    compose(*rootGroup_);
  }
  virtual ~Scene()
  {
    delete rootGroup_;
    delete context_;
  }
  SceneNodeGroup *getRootGroup() { return rootGroup_; }
  const SceneNodeGroup *getRootGroup() const { return rootGroup_; }

  // --- SceneContext管理 ---
  void setContext(SceneContext *ctx) { context_ = ctx; }
  SceneContext *getContext() const { return context_; }

protected:
  SceneNodeGroup *rootGroup_;
  SceneContext *context_;
  MutableState<ScenePhase> currentPhase;

public:
  // --- UI構成宣言API ---
  virtual void compose(SceneNodeGroup &group) {}

  // --- シーン切り替えフック ---
  virtual void onAttach() { currentPhase.set(ATTACHED); }
  virtual void onDetach() { currentPhase.set(DETACHED); }

  // 外部向け: State<ScenePhase>参照を返す
  const State<ScenePhase> &phase() const { return currentPhase; }

  // --- API拡張例 ---
  // // シリアライズ・リセット等の拡張もここで追加可能
  // virtual std::string serialize() const { /* ... */ }
  // virtual void reset() { /* ... */ }
};

#endif // DECLARA_SCENE_HPP
