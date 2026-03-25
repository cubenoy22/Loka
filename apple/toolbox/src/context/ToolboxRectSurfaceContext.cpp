#include "context/ToolboxRectSurfaceContext.hpp"

ToolboxRectSurfaceContext::ToolboxRectSurfaceContext(loka::app::RectSurfaceNode *node)
    : node_(node),
      boundary_(0),
      rect_()
{
  SetRect(&rect_, 0, 0, 0, 0);
}

ToolboxRectSurfaceContext::~ToolboxRectSurfaceContext()
{
}

short ToolboxRectSurfaceContext::layout(loka::app::scene::IPlatformController *,
                                        loka::app::scene::LayoutState &state)
{
  if (!node_)
  {
    return 0;
  }
  rect_.left = state.x;
  rect_.top = static_cast<short>(state.y);
  rect_.right = static_cast<short>(state.x + node_->props.width_);
  rect_.bottom = static_cast<short>(state.y + node_->props.height_);
  state.y = static_cast<short>(rect_.bottom + state.spacing);
  return node_->props.width_;
}

void ToolboxRectSurfaceContext::render(loka::app::scene::IPlatformController *)
{
  if (!node_ || !node_->props.model_)
  {
    return;
  }
  const loka::app::RectSurfaceModel model = node_->props.model_->get();
  for (short i = 0; i < model.rectCount; ++i)
  {
    Rect spriteRect;
    spriteRect.left = static_cast<short>(rect_.left + model.rects[i].x);
    spriteRect.top = static_cast<short>(rect_.top + model.rects[i].y);
    spriteRect.right = static_cast<short>(spriteRect.left + model.rects[i].width);
    spriteRect.bottom = static_cast<short>(spriteRect.top + model.rects[i].height);
    PaintRect(&spriteRect);
  }
}
