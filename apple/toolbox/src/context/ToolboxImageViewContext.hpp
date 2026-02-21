#ifndef LOKA_TOOLBOX_IMAGE_VIEW_CONTEXT_HPP
#define LOKA_TOOLBOX_IMAGE_VIEW_CONTEXT_HPP

#include "app/ImageView.hpp"

class ToolboxImageViewContext : public loka::app::scene::NodeContext
{
public:
  explicit ToolboxImageViewContext(loka::app::ImageViewNode *node);
  virtual ~ToolboxImageViewContext();

  virtual short layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state);

private:
  loka::app::ImageViewNode *node_;
};

#endif // LOKA_TOOLBOX_IMAGE_VIEW_CONTEXT_HPP
