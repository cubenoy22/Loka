#ifndef DECLARA_TEXT_HPP
#define DECLARA_TEXT_HPP

#include <string>
#include "core/Component.hpp"
#include "core/PlatformContext.hpp"
#include "app/Button.hpp" // Component2基底クラスを参照

class TextComponent : public Component2
{
public:
  TextComponent(const std::string &text) : text_(text) {}
  void updateStates() {}

private:
  std::string text_;
};

#endif // DECLARA_TEXT_HPP
