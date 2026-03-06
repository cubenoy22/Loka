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
  int sizePolicy = loka::app::IMAGE_VIEW_SIZE_AUTO;
  int width = state.width;
  int height = state.lineHeight > 0 ? state.lineHeight : 80;
  if (node_)
  {
    const bool hasExplicitWidth = node_->props.width_ > 0;
    const bool hasExplicitHeight = node_->props.height_ > 0;
    if (node_->props.hasAttr_ && node_->props.attr_.hasSizePolicyValue_)
    {
      sizePolicy = static_cast<int>(node_->props.attr_.sizePolicyValue_);
    }

    if (hasExplicitWidth)
    {
      width = node_->props.width_;
    }
    if (hasExplicitHeight)
    {
      height = node_->props.height_;
    }

    int srcWidth = 0;
    int srcHeight = 0;
    if (node_->props.image_)
    {
      const loka::core::resource::Image current = node_->props.image_->get();
      srcWidth = current.width();
      srcHeight = current.height();
    }

    if (sizePolicy == loka::app::IMAGE_VIEW_SIZE_INTRINSIC && !hasExplicitWidth && srcWidth > 0)
    {
      width = srcWidth;
    }
    else if (sizePolicy == loka::app::IMAGE_VIEW_SIZE_FILL_PARENT && !hasExplicitWidth)
    {
      width = state.width;
    }

    if (!hasExplicitHeight)
    {
      if (sizePolicy == loka::app::IMAGE_VIEW_SIZE_FILL_PARENT && state.height > 0)
      {
        height = state.height;
      }
      else if (srcWidth > 0 && srcHeight > 0 && width > 0)
      {
        height = (width * srcHeight) / srcWidth;
      }
      else if (srcHeight > 0)
      {
        height = srcHeight;
      }
    }
  }
  state.y = static_cast<short>(state.y + static_cast<short>(height) + state.spacing);
  return static_cast<short>(width);
}
