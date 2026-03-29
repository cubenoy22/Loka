#ifndef LOKA_MAC_IMAGE_VIEW_CONTEXT_HPP
#define LOKA_MAC_IMAGE_VIEW_CONTEXT_HPP

#include "app/ImageView.hpp"
#include "loka/core/State.hpp"
#include "core/resource/Image.hpp"

class MacImageViewContext : public loka::app::scene::NodeContext
{
public:
  MacImageViewContext(void *parentView, int x, int y, int width, int height, loka::app::ImageViewNode *node);
  virtual ~MacImageViewContext();
  virtual short layout(loka::app::scene::IPlatformController *controller, loka::app::scene::LayoutState &state);
  void relayout(int x, int y, int width, int height);

private:
  void bindImage();
  void unbindImage();
  void applyImage();

  static void ImageChangedThunk(void *userData);

  loka::app::ImageViewNode *node_;
  void *imageView_;
  loka::core::State<loka::core::resource::Image> *imageState_;
  loka::core::resource::Image image_;
};

#endif // LOKA_MAC_IMAGE_VIEW_CONTEXT_HPP
