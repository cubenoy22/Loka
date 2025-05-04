#ifndef DECLARA_PAGE_HPP
#define DECLARA_PAGE_HPP

#include "Property.hpp"
#include "Button.hpp"
#include "Text.hpp"
#include "TextInput.hpp"
#include <string>
#include <vector>

class Window;
class Renderer;

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

class Page
{
public:
  Page(Window *hostWindow = 0)
      : window_(hostWindow) {}

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

  // buildは純粋仮想関数に
  virtual void build(PageBuilder &b) = 0;

protected:
  std::vector<Component *> components_;
  Window *window_;
};

#endif // DECLARA_PAGE_HPP
