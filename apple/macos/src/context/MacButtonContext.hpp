#ifndef LOKA_MAC_BUTTON_CONTEXT_HPP
#define LOKA_MAC_BUTTON_CONTEXT_HPP

#include "core2/scene/NativeNodeContext.hpp"
#include "core/State.hpp"

namespace declara
{
  namespace app
  {
    class ButtonNode;
  }
}

class MacButtonContext : public declara::core::scene::NativeNodeContext
{
public:
  MacButtonContext(void *parentView, int x, int y, int width, int height, declara::app::ButtonNode *node);
  virtual ~MacButtonContext();

  void handlePress();

private:
  void bindText();
  void unbindText();
  void applyText();
  static void TextChangedThunk(void *userData);

  declara::app::ButtonNode *node_;
  void *button_;
  void *target_;
  State<std::string> *textState_;
};

#endif // LOKA_MAC_BUTTON_CONTEXT_HPP
