#ifndef DECLARA_TEXTINPUT_HPP
#define DECLARA_TEXTINPUT_HPP

#include <string>
#include "core/PlatformContext.hpp"
#include "core/Component.hpp"
#include "Button.hpp" // Component2基底クラスを参照

class TextInputComponent : public Component2
{
public:
  TextInputComponent(State<std::string> *state) : state_(state) {}
  void updateStates() {}

private:
  State<std::string> *state_;
};

#endif // DECLARA_TEXTINPUT_HPP
