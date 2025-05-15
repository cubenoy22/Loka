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
#include "core/TreedSceneComponent.hpp"
#include "core/SolidTreeSceneComponent.hpp"
#include "core/SceneContext.hpp"

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
  // 今後: TreedSceneComponent/Leaf/Box等の追加APIを拡張
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
  // Solid.js風ツリー型UIノードの追加（compose関数でネスト可）
  SceneBuilder &SolidTree(void (*composeFn)(SolidTreeSceneComponent &) = 0)
  {
    SolidTreeSceneComponent *tree = composeFn ? new SolidTreeSceneComponent(composeFn) : new SolidTreeSceneComponent();
    components.push_back(tree);
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
      : group_(nullptr), currentPhase(DETACHED), context_(nullptr)
  {
    SceneBuilder builder;
    compose(builder);
    group_ = builder.buildPtr();
  }
  virtual ~Scene()
  {
    delete group_;
    delete context_;
  }
  const ComponentGroup<SceneComponent> *getComponentGroup() const { return group_; }

  // --- SceneContext管理 ---
  void setContext(SceneContext *ctx) { context_ = ctx; }
  SceneContext *getContext() const { return context_; }

protected:
  ComponentGroup<SceneComponent> *group_;
  SceneContext *context_;
  MutableState<ScenePhase> currentPhase;

public:
  // --- UI構成宣言API ---
  virtual void compose(SceneBuilder &b) {}

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
