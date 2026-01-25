#ifndef LOKA_WIN32_CELL_CONTEXT_HPP
#define LOKA_WIN32_CELL_CONTEXT_HPP

#include <windows.h>
#include "core2/scene/NativeNodeContext.hpp"
#include "loka/core/String.hpp"
#include <string>

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
    class CellNode;
  }
}

class Win32CellContext : public loka::core::scene::NativeNodeContext
{
public:
  Win32CellContext(HWND parent, int x, int y, int width, int height, loka::app::CellNode *node);
  virtual ~Win32CellContext();

private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  static void EnsureClassRegistered();
  void bindText();
  void unbindText();
  void applyText();
  void drawCell(HDC hdc, const RECT &rect);
  static void TextChangedThunk(void *userData);

  loka::app::CellNode *node_;
  HWND hwnd_;
  loka::core::State<loka::core::String> *textState_;
  std::string text_;
};

#endif // LOKA_WIN32_CELL_CONTEXT_HPP
