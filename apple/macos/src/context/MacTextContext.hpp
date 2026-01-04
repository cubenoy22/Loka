#ifndef LOKA_MAC_TEXT_CONTEXT_HPP
#define LOKA_MAC_TEXT_CONTEXT_HPP

#include "core2/scene/NativeNodeContext.hpp"
#include "core/State.hpp"

namespace declara
{
  namespace app
  {
    class TextNode;
  }
}

class MacTextContext : public declara::core::scene::NativeNodeContext
{
public:
  MacTextContext(void *parentView, int x, int y, int width, int height, declara::app::TextNode *node);
  virtual ~MacTextContext();

private:
  void bindText();
  void unbindText();
  void applyText();
  static void TextChangedThunk(void *userData);

  declara::app::TextNode *node_;
  void *label_;
  State<std::string> *textState_;
};

#endif // LOKA_MAC_TEXT_CONTEXT_HPP
