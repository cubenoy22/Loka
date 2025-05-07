#ifndef DECLARA_SCENE_HPP
#define DECLARA_SCENE_HPP

#include "app/Button.hpp"
#include "app/Text.hpp"
#include "app/TextInput.hpp"
#include <string>
#include <vector>
#include "core/ComponentGroup.hpp"
#include "core/ComponentGroupBuilder.hpp"

class PlatformContext;

// SceneHost: Sceneのホスト役。相互参照・include地獄回避のためのラッパー。
class SceneHost;

/**
 * SceneBuilder: UIや世界の状態を宣言的に構築するビルダー。
 * Scene: "ページ"に限定されない、抽象的な「世界」や「状態」の単位。
 *   - 画面遷移やアプリの状態だけでなく、ゲームや仮想空間など多様な「シーン」表現に応用可能な設計。
 *   - compose(SceneBuilder&) でシーン内の構成要素を宣言する。
 */
class SceneBuilder : public ComponentGroupBuilder<Component2>
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
  Scene(SceneHost *host)
      : group_(0)
  {
    SceneBuilder builder;
    compose(builder);
    group_ = builder.buildPtr();
  }
  virtual ~Scene() { delete group_; }
  const ComponentGroup<Component2> *getComponentGroup() const { return group_; }
  // --- ライフサイクルフック ---
  virtual void onCreate() {}
  virtual void onAttach() {}
  virtual void onDetach() {}
  virtual void onDestroy() {}
  // virtual bool isDiscardable() const { return true; }
  // virtual void requestDiscard(DiscardCallback *callback)
  // {
  //   if (callback)
  //     (*callback)(true);
  // }
  // virtual void onDiscardRequestAborted() {}
  // --- UI構成宣言API ---
  virtual void compose(SceneBuilder &b) {}
  // --- API拡張例 ---
  // // シリアライズ・リセット等の拡張もここで追加可能
  // virtual std::string serialize() const { /* ... */ }
  // virtual void reset() { /* ... */ }
protected:
  ComponentGroup<Component2> *group_;
};

#endif // DECLARA_SCENE_HPP
