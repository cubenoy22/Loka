#ifndef LOKA_WIN32_EDIT_TEXT_CONTEXT_HPP
#define LOKA_WIN32_EDIT_TEXT_CONTEXT_HPP

#include <windows.h>
#include <string>
#include "core2/scene/NativeNodeContext.hpp"

template <typename T>
class State;
template <typename T>
class MutableState;

namespace declara
{
  namespace app
  {
    class EditTextNode;
  }
}

class Win32EditTextContext : public declara::core::scene::NativeNodeContext
{
public:
  Win32EditTextContext(HWND parent, int x, int y, int width, int height, declara::app::EditTextNode *node);
  virtual ~Win32EditTextContext();

  HWND hwnd() const { return hwnd_; }
  bool handleCommand(WPARAM wParam, LPARAM lParam);

private:
  void bindText();
  void unbindText();
  void applyText();
  void syncStateFromControl();
  static void TextChangedThunk(void *userData);

  declara::app::EditTextNode *node_;
  HWND hwnd_;
  State<std::string> *textState_;
  bool applyingFromState_;
  bool updatingFromControl_;
};

#endif // LOKA_WIN32_EDIT_TEXT_CONTEXT_HPP
