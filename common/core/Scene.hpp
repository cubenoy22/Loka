#ifndef DECLARA_SCENE_HPP
#define DECLARA_SCENE_HPP

#include "app/Button.hpp"
#include "app/Text.hpp"
#include "app/TextInput.hpp"
#include <string>
#include <vector>

class Window;
class PlatformContext;

/**
 * SceneBuilder: UIや世界の状態を宣言的に構築するビルダー。
 * Scene: "ページ"に限定されない、抽象的な「世界」や「状態」の単位。
 *   - 画面遷移やアプリの状態だけでなく、ゲームや仮想空間など多様な「シーン」表現に応用可能な設計。
 *   - build(SceneBuilder&) でシーン内の構成要素を宣言する。
 */
class SceneBuilder
{
public:
  SceneBuilder() {}
  void Text(const std::string &text)
  {
    components.push_back(new TextComponent(text));
  }
  void TextInput(State<std::string> *state)
  {
    components.push_back(new TextInputComponent(state));
  }
  void Button(const ButtonOptions &opts)
  {
    components.push_back(new ButtonComponent(opts.label, opts.enabled, opts.onClick));
  }
  std::vector<Component *> build()
  {
    std::vector<Component *> tmp;
    tmp.swap(components);
    return tmp;
  }
  ~SceneBuilder()
  {
    for (size_t i = 0; i < components.size(); ++i)
      delete components[i];
  }

private:
  std::vector<Component *> components;
};

class Scene
{
public:
  Scene(Window *hostWindow = 0)
      : window_(hostWindow) {}

  virtual ~Scene()
  {
    for (size_t i = 0; i < components_.size(); ++i)
      delete components_[i];
  }
  Window *getHostWindow() const { return window_; }
  void buildContext()
  {
    SceneBuilder b;
    build(b);
    std::vector<Component *> tmp = b.build();
    components_.swap(tmp);
  }

  // buildは純粋仮想関数に
  virtual void build(SceneBuilder &b) = 0;

protected:
  std::vector<Component *> components_;
  Window *window_;
};

#endif // DECLARA_SCENE_HPP
