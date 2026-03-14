#ifndef LOKA_WIN32_EDIT_TEXT_CONTEXT_HPP
#define LOKA_WIN32_EDIT_TEXT_CONTEXT_HPP

#include <windows.h>
#include "app/scene/NativeNodeContext.hpp"
#include "loka/core/String.hpp"

namespace loka
{
  namespace core
  {
    template <typename T>
    class State;
    template <typename T>
    class MutableState;
  }
}

namespace loka
{
  namespace app
  {
    class EditTextNode;
  }
}

class Win32EditTextContext : public loka::app::scene::NativeNodeContext
{
public:
  Win32EditTextContext(HWND parent, int x, int y, int width, int height, loka::app::EditTextNode *node);
  virtual ~Win32EditTextContext();

  HWND hwnd() const { return hwnd_; }
  void relayout(int x, int y, int width, int height);
  bool handleCommand(WPARAM wParam, LPARAM lParam);

private:
  void bindText();
  void unbindText();
  void applyText();
  void syncStateFromControl();
  static void TextChangedThunk(void *userData);

  loka::app::EditTextNode *node_;
  HWND hwnd_;
  loka::core::State<loka::core::String> *textState_;
  bool applyingFromState_;
  bool updatingFromControl_;
};

#endif // LOKA_WIN32_EDIT_TEXT_CONTEXT_HPP
