#include "context/ToolboxImageViewContext.hpp"

ToolboxImageViewContext::ToolboxImageViewContext(loka::app::ImageViewNode *node)
    : node_(node)
{
}

ToolboxImageViewContext::~ToolboxImageViewContext()
{
}

short ToolboxImageViewContext::layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state)
{
  short width = state.width;
  short height = state.lineHeight > 0 ? state.lineHeight : 80;
  if (node_)
  {
    if (node_->props.width_ > 0)
    {
      width = static_cast<short>(node_->props.width_);
    }
    if (node_->props.height_ > 0)
    {
      height = static_cast<short>(node_->props.height_);
    }
    else if (node_->props.image_)
    {
      const loka::core::resource::Image current = node_->props.image_->get();
      const int srcWidth = current.width();
      const int srcHeight = current.height();
      if (srcWidth > 0 && srcHeight > 0 && width > 0)
      {
        height = static_cast<short>((width * srcHeight) / srcWidth);
      }
      else if (srcHeight > 0)
      {
        height = static_cast<short>(srcHeight);
      }
    }
  }
  state.y = static_cast<short>(state.y + height + state.spacing);
  return width;
}
