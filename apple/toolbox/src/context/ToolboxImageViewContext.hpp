#ifndef LOKA_TOOLBOX_IMAGE_VIEW_CONTEXT_HPP
#define LOKA_TOOLBOX_IMAGE_VIEW_CONTEXT_HPP

#include "app/ImageView.hpp"
#include "core/resource/Image.hpp"
#include "loka/core/State.hpp"
#include <Quickdraw.h>

class ToolboxImageViewContext : public loka::app::scene::NodeContext
{
public:
  explicit ToolboxImageViewContext(loka::app::ImageViewNode *node);
  virtual ~ToolboxImageViewContext();

  virtual short layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state);
  virtual void render(loka::app::scene::IPlatformController *);

  void setBoundary(loka::app::scene::BoundaryNode *boundary) { boundary_ = boundary; }

private:
  void draw();
  void updateRect(short x, short y, short width, short height);

  loka::app::ImageViewNode *node_;
  loka::app::scene::BoundaryNode *boundary_;
  Rect rect_;
  loka::core::resource::Image image_;
};

#endif // LOKA_TOOLBOX_IMAGE_VIEW_CONTEXT_HPP
