#ifndef LOKA_WIN32_BUTTON_CONTEXT_HPP
#define LOKA_WIN32_BUTTON_CONTEXT_HPP

#include <windows.h>
#include "app/scene/NativeNodeContext.hpp"
#include "loka/core/String.hpp"

namespace loka
{
  namespace core
  {
    template <typename T>
    class State;
    class EmitterState;
  }
}

namespace loka
{
  namespace app
  {
    class ButtonNode;
  }
}

class Win32ButtonContext : public loka::app::scene::NativeNodeContext,
                           public loka::app::scene::ICapturableBitmap
{
public:
  Win32ButtonContext(HWND parent, int x, int y, int width, int height, loka::app::ButtonNode *node);
  virtual ~Win32ButtonContext();
  virtual loka::app::scene::ICapturableBitmap *asCapturableBitmap() { return this; }
  virtual const loka::app::scene::ICapturableBitmap *asCapturableBitmap() const { return this; }
  virtual bool captureBitmap(loka::core::resource::Image &out) const;

  HWND hwnd() const { return hwnd_; }
  void relayout(int x, int y, int width, int height);
  bool handleCommand(WPARAM wParam, LPARAM lParam);

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
  HWND hwnd_;
  loka::core::State<loka::core::String> *textState_;
  loka::core::State<bool> *enabledState_;
};

#endif // LOKA_WIN32_BUTTON_CONTEXT_HPP
