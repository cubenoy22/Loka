#ifndef LOKA_MAC_IMAGE_VIEW_CONTEXT_HPP
#define LOKA_MAC_IMAGE_VIEW_CONTEXT_HPP

#include "app/nodes/ImageView.hpp"
#include "core/State.hpp"
#include "core/resource/Image.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class PlatformNodeHandlerRegistry;
    }
  } // namespace app
} // namespace loka

class MacImageViewContext : public loka::app::scene::NodeContext
{
public:
  MacImageViewContext(void *parentView, int x, int y, int width, int height, loka::app::ImageViewNode *node);
  virtual ~MacImageViewContext();
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);
  virtual void onNodeAttached();
  virtual void onNodeDetached();
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                             loka::app::scene::NodeLifecycleFact next);
  void relayout(int x, int y, int width, int height);

private:
  void applyAttachedPresentation();
  void applyDetachedPresentation();
  void bindImage();
  void unbindImage();
  void applyImage();

  static void ImageChangedThunk(void *userData);

  loka::app::ImageViewNode *node_;
  void *imageView_;
  loka::core::State<loka::core::resource::Image> *imageState_;
  loka::core::resource::Image image_;
};

void RegisterMacImageViewNodeHandler(loka::app::scene::PlatformNodeHandlerRegistry &registry);

#endif // LOKA_MAC_IMAGE_VIEW_CONTEXT_HPP
