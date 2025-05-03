#ifndef DECLARA_PAGE_HPP
#define DECLARA_PAGE_HPP

#include "App.hpp"
#include "Property.hpp"
#include "Transaction.hpp"
#include "Button.hpp"
#include "Text.hpp"
#include "TextInput.hpp"
#include <string>
#include <vector>

class Window;
class Renderer;

class Page
{
public:
  Page(Window *hostWindow = 0)
      : window_(hostWindow), transaction_() {}

  virtual ~Page()
  {
    for (size_t i = 0; i < components_.size(); ++i)
      delete components_[i];
  }
  Window *getHostWindow() const { return window_; }
  void buildContext()
  {
    PageBuilder b;
    build(b);
    std::vector<Component *> tmp = b.build();
    components_.swap(tmp);
  }
  void renderAll(Renderer *renderer)
  {
    for (size_t i = 0; i < components_.size(); ++i)
      components_[i]->render(renderer);
  }

  // ページ内の Transaction を commit するラッパー
  virtual bool commitTransaction() { return transaction_.commit(); }

  // buildは純粋仮想関数に
  virtual void build(PageBuilder &b) = 0;

protected:
  Transaction transaction_;
  std::vector<Component *> components_;
  Window *window_;
};

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
  std::vector<Component *> build()
  {
    std::vector<Component *> tmp;
    tmp.swap(components);
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

#endif // DECLARA_PAGE_HPP
