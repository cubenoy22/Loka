#ifndef DECLARA_TEXTINPUT_HPP
#define DECLARA_TEXTINPUT_HPP

#include <string>
#include "Renderer.hpp"
#include "Property.hpp"
#include "Button.hpp" // Component基底クラスを参照

class TextInputComponent : public Component
{
public:
  TextInputComponent(State<std::string> *state) : state_(state) {}
  void render(Renderer *renderer)
  {
    // 仮実装: renderer->createTextInput(state_);
  }
  void updateProps() {}

private:
  State<std::string> *state_;
};

#endif // DECLARA_TEXTINPUT_HPP
