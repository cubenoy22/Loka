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
class Page
{
public:
  Page() {}
  virtual void build(PageBuilder &b) = 0;
  virtual std::string serialize() { return ""; }
  virtual ~Page() {}

  // UIContextを渡してPageContextを再構築
  void buildContext(UIContext *ctx)
  {
    PageBuilder b;
    build(b);
    context_ = PageContext(b.buildContextValue(ctx)); // 値型でswap
  }
  void renderAll() { context_.renderAll(); }

protected:
  PageContext context_;
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
  PageContext *buildContext(UIContext *ctx)
  {
    return new PageContext(std::move(components), ctx);
  }

  // PageContextを値型で返す
  PageContext buildContextValue(UIContext *ctx)
  {
    return PageContext(std::move(components), ctx);
  }

  // build中はUIContextや描画処理は一切呼ばないこと（設計意図）

  ~PageBuilder()
  {
    for (size_t i = 0; i < components.size(); ++i)
      delete components[i];
  }

private:
  std::vector<Component *> components;
};

// PageContext: build後のUI部品を保持し、描画やUIContextとのやりとりを担当
class PageContext
{
public:
  PageContext() : context_(0) {}
  PageContext(std::vector<Component *> components, UIContext *ctx)
      : components_(components), context_(ctx) {}
  void renderAll()
  {
    for (size_t i = 0; i < components_.size(); ++i)
      components_[i]->render(context_->renderer);
  }
  void swap(PageContext &other)
  {
    components_.swap(other.components_);
    UIContext *tmp = context_;
    context_ = other.context_;
    other.context_ = tmp;
  }
  ~PageContext()
  {
    for (size_t i = 0; i < components_.size(); ++i)
      delete components_[i];
  }

private:
  // components_はbuild後にmoveで受け取り、以降は絶対に変更しない（immutable設計意図）
  std::vector<Component *> components_;
  UIContext *context_;
};
