#ifndef LOKA_WIN32_TEXT_CONTEXT_HPP
#define LOKA_WIN32_TEXT_CONTEXT_HPP

#include <windows.h>
#include "app/scene/NativeNodeContext.hpp"
#include "loka/core/String.hpp"

namespace loka
{
  namespace core
  {
    template <typename T>
    class State;
  }
}

namespace loka
{
  namespace app
  {
    class TextNode;
  }
}

class Win32TextContext : public loka::app::scene::NativeNodeContext
{
public:
  Win32TextContext(HWND parent, int x, int y, int width, int height, loka::app::TextNode *node);
  virtual ~Win32TextContext();

private:
  void bindText();
  void unbindText();
  void applyText();
  static void TextChangedThunk(void *userData);

  loka::app::TextNode *node_;
  HWND hwnd_;
  loka::core::State<loka::core::String> *textState_;
};

#endif // LOKA_WIN32_TEXT_CONTEXT_HPP
