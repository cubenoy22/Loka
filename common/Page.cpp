// SnapshotベースのシンプルなUIフレームワーク仕様（C++）

#include "App.hpp"
#include "Property.hpp"
#include "Transaction.hpp"
#include "Button.hpp"
#include "Text.hpp"
#include "TextInput.hpp"
#include <string>
#include <vector>

// ==============================
// 基本クラス・コンセプト
// ==============================

// Page: 画面単位の抽象基底クラス
class Window; // 追加: Window前方宣言
class Page
{
public:
  Page(Window *hostWindow = 0) : window_(hostWindow) {}
  virtual void build(PageBuilder &b) = 0;
  virtual std::string serialize() { return ""; }
  virtual ~Page()
  {
    for (size_t i = 0; i < components_.size(); ++i)
      delete components_[i];
  }

  // Window参照のgetter（操作はしない設計）
  Window *getHostWindow() const { return window_; }

  // UIContextを渡してPageContextを再構築
  void buildContext()
  {
    PageBuilder b;
    build(b);
    // C++98ではmove不可、swapで所有権を移譲
    std::vector<Component *> tmp = b.build();
    components_.swap(tmp);
  }
  void renderAll(Renderer *renderer)
  {
    for (size_t i = 0; i < components_.size(); ++i)
      components_[i]->render(renderer);
  }

protected:
  std::vector<Component *> components_;
  Window *window_; // 追加: 自分をホストするWindow参照
};

// ==============================
// PageBuilder: UI部品の登録専用（create系のみ公開）
// ==============================

class PageBuilder
{
public:
  PageBuilder() {}

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

  // PageContextを生成し、内部componentsをmoveで渡す
  std::vector<Component *> build()
  {
    std::vector<Component *> tmp;
    tmp.swap(components); // C++98流の所有権移譲
    return tmp;
  }

  ~PageBuilder()
  {
    for (size_t i = 0; i < components.size(); ++i)
      delete components[i];
  }

private:
  std::vector<Component *> components;
};
