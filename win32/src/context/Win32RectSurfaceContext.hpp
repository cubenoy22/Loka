#ifndef LOKA_WIN32_RECT_SURFACE_CONTEXT_HPP
#define LOKA_WIN32_RECT_SURFACE_CONTEXT_HPP

#include <windows.h>
#include "app/RectSurface.hpp"
#include "Win32RetirableContext.hpp"

namespace loka
{
  namespace app
  {
    class RectSurfaceNode;
  }
} // namespace loka

class Win32ScenePlatformController;

class Win32RectSurfaceContext : public Win32RetirableContext
{
public:
  Win32RectSurfaceContext(Win32ScenePlatformController *controller,
                          HWND parent,
                          int x,
                          int y,
                          int width,
                          int height,
                          loka::app::RectSurfaceNode *node);
  virtual ~Win32RectSurfaceContext();
  /** Attach-time read (late-subscriber rule): presentation from the current
      fact, called by the installing handler right after setContext. */
  void readLifecycleFactOnAttach();
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
  bool queryBoundsInParent(HWND &parent, RECT &rect) const;
  loka::app::RectSurfacePaintResult draw(HDC hdc, const RECT &rect, RECT &retryRect);
  void finishPaint(loka::app::RectSurfacePaintResult result, const RECT &retryRect);

  loka::app::RectSurfaceNode *node_;
  HWND hwnd_;
  loka::core::State<loka::app::RectSurfaceModel> *modelState_;
};

#endif
