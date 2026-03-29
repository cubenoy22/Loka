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

class MacButtonContext : public loka::app::scene::NativeNodeContext,
                         public loka::app::scene::ICapturableBitmap
{
public:
  MacButtonContext(void *parentView, int x, int y, int width, int height, loka::app::ButtonNode *node);
  virtual ~MacButtonContext();
  virtual loka::app::scene::ICapturableBitmap *asCapturableBitmap() { return this; }
  virtual const loka::app::scene::ICapturableBitmap *asCapturableBitmap() const { return this; }
  virtual bool captureBitmap(loka::core::resource::Image &out) const;
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);

  void handlePress();
  void relayout(int x, int y, int width, int height);

private:
  void bindText();
  void unbindText();
  void bindEnabled();
  void unbindEnabled();
  void applyText();
  void applyEnabled();
  static void TextChangedThunk(void *userData);
  static void EnabledChangedThunk(void *userData);

  loka::app::ButtonNode *node_;
  void *button_;
  void *target_;
  loka::core::State<loka::core::String> *textState_;
  loka::core::State<bool> *enabledState_;
};

#endif // LOKA_MAC_BUTTON_CONTEXT_HPP
