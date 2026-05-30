#ifndef LOKA_WIN32_TEXT_CONTEXT_HPP
#define LOKA_WIN32_TEXT_CONTEXT_HPP

#include <windows.h>
#include "app/scene/projection/NativeNodeContext.hpp"
#include "core/String.hpp"

namespace loka
{
  namespace core
  {
    template <typename T> class State;
  }
} // namespace loka

namespace loka
{
  namespace app
  {
    class TextNode;
    namespace scene
    {
      class PlatformNodeHandlerRegistry;
    }
  } // namespace app
} // namespace loka

class Win32TextContext : public loka::app::scene::NativeNodeContext, public loka::app::scene::ICapturableBitmap
{
public:
  Win32TextContext(HWND parent, int x, int y, int width, int height, loka::app::TextNode *node);
  virtual ~Win32TextContext();
  virtual loka::app::scene::ICapturableBitmap *asCapturableBitmap()
  {
    return this;
  }
  virtual const loka::app::scene::ICapturableBitmap *asCapturableBitmap() const
  {
    return this;
  }
  virtual bool captureBitmap(loka::core::resource::Image &out) const;
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);
  virtual void onNodeAttached();
  virtual void onNodeDetached();
  void relayout(int x, int y, int width, int height);

private:
  void bindText();
  void unbindText();
  void applyText();
  void requestRelayoutIfNeeded();
  static void TextChangedThunk(void *userData);

  loka::app::TextNode *node_;
  HWND hwnd_;
  loka::core::State<loka::core::String> *textState_;
  bool didInitialApply_;
};

void RegisterWin32TextNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_WIN32_TEXT_CONTEXT_HPP
