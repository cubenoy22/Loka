#ifndef DECLARA_WIN32_TEXT_CONTEXT_HPP
#define DECLARA_WIN32_TEXT_CONTEXT_HPP

#include <windows.h>
#include <string>
#include "core2/scene/NativeNodeContext.hpp"

template <typename T>
class State;

namespace declara
{
  namespace app
  {
    class TextNode;
  }
}

class Win32TextContext : public declara::core::scene::NativeNodeContext
{
public:
  Win32TextContext(HWND parent, int x, int y, int width, int height, declara::app::TextNode *node);
  virtual ~Win32TextContext();

private:
  void bindText();
  void unbindText();
  void applyText();
  static void TextChangedThunk(void *userData);

  declara::app::TextNode *node_;
  HWND hwnd_;
  State<std::string> *textState_;
};

#endif // DECLARA_WIN32_TEXT_CONTEXT_HPP
