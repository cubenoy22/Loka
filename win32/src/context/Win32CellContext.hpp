#ifndef LOKA_WIN32_CELL_CONTEXT_HPP
#define LOKA_WIN32_CELL_CONTEXT_HPP

#include <windows.h>
#include "core2/scene/NativeNodeContext.hpp"
#include "loka/core/String.hpp"
#include <string>

template <typename T>
class State;

namespace declara
{
  namespace app
  {
    class CellNode;
  }
}

class Win32CellContext : public declara::core::scene::NativeNodeContext
{
public:
  Win32CellContext(HWND parent, int x, int y, int width, int height, declara::app::CellNode *node);
  virtual ~Win32CellContext();

private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  static void EnsureClassRegistered();
  void bindText();
  void unbindText();
  void applyText();
  void drawCell(HDC hdc, const RECT &rect);
  static void TextChangedThunk(void *userData);

  declara::app::CellNode *node_;
  HWND hwnd_;
  State<loka::core::String> *textState_;
  std::string text_;
};

#endif // LOKA_WIN32_CELL_CONTEXT_HPP
