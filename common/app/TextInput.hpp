#ifndef DECLARA_TEXTINPUT_HPP
#define DECLARA_TEXTINPUT_HPP

#include <string>
#include "app/PlatformContext.hpp"
#include "Button.hpp" // Component基底クラスを参照

class TextInputComponent : public Component
{
public:
  TextInputComponent(State<std::string> *state) : state_(state) {}
  void render(PlatformContext *context)
  {
    // 仮実装: context->createTextInput(state_);
  }
  void updateStates() {}

private:
  State<std::string> *state_;
};

#endif // DECLARA_TEXTINPUT_HPP
