#ifndef LOKA_MAC_RECT_SURFACE_CONTEXT_HPP
#define LOKA_MAC_RECT_SURFACE_CONTEXT_HPP

#include "app/RectSurface.hpp"
#include "app/scene/projection/NativeNodeContext.hpp"

namespace loka
{
  namespace app
  {
    class RectSurfaceNode;
  }
}

class MacRectSurfaceContext : public loka::app::scene::NativeNodeContext
{
public:
  MacRectSurfaceContext(void *parentView, int x, int y, int width, int height, loka::app::RectSurfaceNode *node);
  virtual ~MacRectSurfaceContext();
  virtual void onNodeAttached();
  virtual void onNodeDetached();

  void relayout(int x, int y, int width, int height);
  void draw(void *viewBounds);

private:
  static void ModelChangedThunk(void *userData);
  void bindModel();
  void unbindModel();
  void applyModel();

  loka::app::RectSurfaceNode *node_;
  loka::core::State<loka::app::RectSurfaceModel> *modelState_;
  void *view_;
};

#endif
