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
    namespace scene
    {
      class PlatformNodeHandlerRegistry;
    }
  }
}

class MacTextContext : public loka::app::scene::NativeNodeContext,
                       public loka::app::scene::ICapturableBitmap
{
public:
  MacTextContext(void *parentView, int x, int y, int width, int height, loka::app::TextNode *node);
  virtual ~MacTextContext();
  virtual loka::app::scene::ICapturableBitmap *asCapturableBitmap() { return this; }
  virtual const loka::app::scene::ICapturableBitmap *asCapturableBitmap() const { return this; }
  virtual bool captureBitmap(loka::core::resource::Image &out) const;
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);
  void relayout(int x, int y, int width, int height);

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

void RegisterMacTextNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_MAC_TEXT_CONTEXT_HPP
