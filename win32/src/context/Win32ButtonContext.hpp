#ifndef LOKA_WIN32_BUTTON_CONTEXT_HPP
#define LOKA_WIN32_BUTTON_CONTEXT_HPP

#include <windows.h>
#include "core2/scene/NativeNodeContext.hpp"
#include "loka/core/String.hpp"

template <typename T>
class State;
class EmitterState;

namespace loka
{
  namespace app
  {
    class ButtonNode;
  }
}

class Win32ButtonContext : public loka::core::scene::NativeNodeContext
{
public:
  Win32ButtonContext(HWND parent, int x, int y, int width, int height, loka::app::ButtonNode *node);
  virtual ~Win32ButtonContext();

  HWND hwnd() const { return hwnd_; }
  bool handleCommand(WPARAM wParam, LPARAM lParam);

private:
  void bindText();
  void unbindText();
  void applyText();
  static void TextChangedThunk(void *userData);

  loka::app::ButtonNode *node_;
  HWND hwnd_;
  State<loka::core::String> *textState_;
};

#endif // LOKA_WIN32_BUTTON_CONTEXT_HPP
