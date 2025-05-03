#ifndef DECLARA_TEXT_HPP
#define DECLARA_TEXT_HPP

#include <string>
#include "Renderer.hpp"
#include "Button.hpp" // Component基底クラスを参照

class TextComponent : public Component
{
public:
  TextComponent(const std::string &text) : text_(text) {}
  void render(Renderer *renderer)
  {
    // 仮実装: renderer->createText(text_);
  }
  void updateProps() {}

private:
  std::string text_;
};

#endif // DECLARA_TEXT_HPP
