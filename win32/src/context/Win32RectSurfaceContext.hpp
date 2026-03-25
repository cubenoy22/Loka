#ifndef LOKA_WIN32_RECT_SURFACE_CONTEXT_HPP
#define LOKA_WIN32_RECT_SURFACE_CONTEXT_HPP

#include <windows.h>
#include "app/RectSurface.hpp"
#include "app/scene/NativeNodeContext.hpp"

namespace loka
{
  namespace app
  {
    class RectSurfaceNode;
  }
}

class Win32RectSurfaceContext : public loka::app::scene::NativeNodeContext
{
public:
  Win32RectSurfaceContext(HWND parent, int x, int y, int width, int height, loka::app::RectSurfaceNode *node);
  virtual ~Win32RectSurfaceContext();

  void relayout(int x, int y, int width, int height);

private:
  static void EnsureClassRegistered();
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  void draw(HDC hdc, const RECT &rect);

  loka::app::RectSurfaceNode *node_;
  HWND hwnd_;
};

#endif
