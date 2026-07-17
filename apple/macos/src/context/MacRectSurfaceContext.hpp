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
} // namespace loka

class MacRectSurfaceContext : public loka::app::scene::NativeNodeContext
{
public:
  MacRectSurfaceContext(void *parentView, int x, int y, int width, int height, loka::app::RectSurfaceNode *node);
  virtual ~MacRectSurfaceContext();
  /** Attach-time read (late-subscriber rule): presentation from the current
      fact, called by the installing handler right after setContext. */
  void readLifecycleFactOnAttach();
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                             loka::app::scene::NodeLifecycleFact next);

  void relayout(int x, int y, int width, int height);
  void draw(void *viewBounds);

private:
  void applyAttachedPresentation();
  void applyDetachedPresentation();
  static void ModelChangedThunk(void *userData);
  void bindModel();
  void unbindModel();
  void applyModel();

  loka::app::RectSurfaceNode *node_;
  loka::core::State<loka::app::RectSurfaceModel> *modelState_;
  void *view_;
};

#endif
