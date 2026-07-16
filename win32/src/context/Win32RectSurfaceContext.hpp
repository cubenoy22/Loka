#ifndef LOKA_WIN32_RECT_SURFACE_CONTEXT_HPP
#define LOKA_WIN32_RECT_SURFACE_CONTEXT_HPP

#include <windows.h>
#include "app/RectSurface.hpp"
#include "app/scene/projection/NativeNodeContext.hpp"

namespace loka
{
  namespace app
  {
    class RectSurfaceNode;
  }
} // namespace loka

class Win32RectSurfaceContext : public loka::app::scene::NativeNodeContext
{
public:
  Win32RectSurfaceContext(HWND parent, int x, int y, int width, int height, loka::app::RectSurfaceNode *node);
  virtual ~Win32RectSurfaceContext();
  virtual void onNodeAttached();
  virtual void onNodeDetached();
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                             loka::app::scene::NodeLifecycleFact next);

  void relayout(int x, int y, int width, int height);

private:
  void applyAttachedPresentation();
  void applyDetachedPresentation();
  static void EnsureClassRegistered();
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
  static void ModelChangedThunk(void *userData);
  void bindModel();
  void unbindModel();
  void applyModel();
  void draw(HDC hdc, const RECT &rect);

  loka::app::RectSurfaceNode *node_;
  HWND hwnd_;
  loka::core::State<loka::app::RectSurfaceModel> *modelState_;
};

#endif
