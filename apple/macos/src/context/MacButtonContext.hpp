#ifndef LOKA_MAC_BUTTON_CONTEXT_HPP
#define LOKA_MAC_BUTTON_CONTEXT_HPP

#include "app/scene/NativeNodeContext.hpp"
#include "loka/core/State.hpp"
#include "loka/core/String.hpp"

namespace loka
{
  namespace app
  {
    class ButtonNode;
  }
}

class MacButtonContext : public loka::app::scene::NativeNodeContext
{
public:
  MacButtonContext(void *parentView, int x, int y, int width, int height, loka::app::ButtonNode *node);
  virtual ~MacButtonContext();

  void handlePress();
  void relayout(int x, int y, int width, int height);

private:
  void bindText();
  void unbindText();
  void applyText();
  static void TextChangedThunk(void *userData);

  loka::app::ButtonNode *node_;
  void *button_;
  void *target_;
  loka::core::State<loka::core::String> *textState_;
};

#endif // LOKA_MAC_BUTTON_CONTEXT_HPP
