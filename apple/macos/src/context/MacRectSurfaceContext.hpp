#ifndef LOKA_MAC_RECT_SURFACE_CONTEXT_HPP
#define LOKA_MAC_RECT_SURFACE_CONTEXT_HPP

#include "app/RectSurface.hpp"
#include "core/Frame.hpp"
#include "MacRetirableContext.hpp"

namespace loka
{
  namespace app
  {
    class RectSurfaceNode;
  }
} // namespace loka

class MacScenePlatformController;

/** Draws one prepared sprite image into the current NSGraphicsContext at the
    sprite rect, upright whether or not that context is flipped (the
    RectSurface view is). Shared by the view's draw and its pin. */
void MacRectSurfaceDrawPreparedImage(void *preparedImage, int x, int y, int width, int height);

/** One RectSurface paint-position's lazy binary-alpha companion. The original
    NSImage remains untouched for ImageView and is held only while cached. */
class MacRectSurfacePreparedImage
{
public:
  MacRectSurfacePreparedImage();
  ~MacRectSurfacePreparedImage();

  void *prepare(const loka::core::resource::Image &source);
  void discardUnless(const loka::core::resource::Image &source);
  void clear();

private:
  MacRectSurfacePreparedImage(const MacRectSurfacePreparedImage &);
  MacRectSurfacePreparedImage &operator=(const MacRectSurfacePreparedImage &);

  loka::core::resource::Image source_;
  void *prepared_;
};

class MacRectSurfaceContext : public MacRetirableContext
{
public:
  MacRectSurfaceContext(MacScenePlatformController *controller,
                        void *parentView,
                        int x,
                        int y,
                        int width,
                        int height,
                        loka::app::RectSurfaceNode *node);
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
  void discardStalePreparedImages();
  void clearPreparedImages();
  void finishPaint(loka::app::RectSurfacePaintResult result, const loka::core::Frame &retryFrame);

  loka::app::RectSurfaceNode *node_;
  loka::core::State<loka::app::RectSurfaceModel> *modelState_;
  void *view_;
  MacRectSurfacePreparedImage preparedImages_[loka::app::RectSurfaceModel::kMaxSprites];
};

#endif
