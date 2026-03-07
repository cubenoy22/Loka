#ifndef LOKA_MAC_TEXT_CONTEXT_HPP
#define LOKA_MAC_TEXT_CONTEXT_HPP

#include "app/scene/NativeNodeContext.hpp"
#include "loka/core/State.hpp"
#include "loka/core/String.hpp"

namespace loka
{
  namespace app
  {
    class TextNode;
  }
}

class MacTextContext : public loka::app::scene::NativeNodeContext
{
public:
  MacTextContext(void *parentView, int x, int y, int width, int height, loka::app::TextNode *node);
  virtual ~MacTextContext();

private:
  void bindText();
  void unbindText();
  void applyText();
  void requestRelayoutIfNeeded();
  static void TextChangedThunk(void *userData);

  loka::app::TextNode *node_;
  void *parentView_;
  void *label_;
  loka::core::State<loka::core::String> *textState_;
  bool didInitialApply_;
};

#endif // LOKA_MAC_TEXT_CONTEXT_HPP
