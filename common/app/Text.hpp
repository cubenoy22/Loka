#ifndef DECLARA_TEXT_HPP
#define DECLARA_TEXT_HPP

#include <string>
#include "app/Renderer.hpp"
#include "app/Button.hpp" // Component基底クラスを参照

class TextComponent : public Component
{
public:
  TextComponent(const std::string &text) : text_(text) {}
  void render(Renderer *renderer)
  {
    // 仮実装: renderer->createText(text_);
  }
  void updateStates() {}

private:
  std::string text_;
};

#endif // DECLARA_TEXT_HPP
