#ifndef DECLARA_SCENE_HPP
#define DECLARA_SCENE_HPP

#include "app/Button.hpp"
#include "app/Text.hpp"
#include "app/TextInput.hpp"
#include <string>
#include <vector>
#include "core/ComponentGroup.hpp"
#include "core/ComponentGroupBuilder.hpp"
#include "core/LayoutEngine.hpp"
#include "core/SceneComponent.hpp"
#include "core/SceneContext.hpp"
#include "core/SceneNodeGroup.hpp"

class PlatformContext;

// SceneHost: Sceneのホスト役。相互参照・include地獄回避のためのラッパー。
class SceneHost;

/**
 * SceneBuilder: UIや世界の状態を宣言的に構築するビルダー。
 * Scene: "ページ"に限定されない、抽象的な「世界」や「状態」の単位。
 *   - 画面遷移やアプリの状態だけでなく、ゲームや仮想空間など多様な「シーン」表現に応用可能な設計。
 *   - compose(SceneBuilder&) でシーン内の構成要素を宣言する。
 */
class SceneBuilder : public ComponentGroupBuilder<SceneComponent>
{
public:
  SceneBuilder() {}
  SceneBuilder &Text(const std::string &text)
  {
    components.push_back(new TextComponent(text));
    return *this;
  }
  SceneBuilder &TextInput(State<std::string> *state)
  {
    components.push_back(new TextInputComponent(state));
    return *this;
  }
  SceneBuilder &Button(const ButtonOptions &opts)
  {
    components.push_back(new ButtonComponent(opts.label, opts.enabled, opts.onClick));
    return *this;
  }
};

// SceneHost: Sceneのホスト役。相互参照・include地獄回避のためのラッパー。
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
