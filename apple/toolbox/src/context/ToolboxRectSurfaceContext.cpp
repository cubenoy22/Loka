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
  if (node_->props.clearBackground_)
  {
    EraseRect(&rect_);
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

void ToolboxRectSurfaceContext::renderDirty(const Rect &dirtyRect)
{
  if (!node_ || !node_->props.model_)
  {
    return;
  }
  if (dirtyRect.right < rect_.left || dirtyRect.left > rect_.right ||
      dirtyRect.bottom < rect_.top || dirtyRect.top > rect_.bottom)
  {
    return;
  }

  const loka::app::RectSurfaceModel model = node_->props.model_->get();

  Rect clearRect = dirtyRect;
  if (clearRect.left < rect_.left)
  {
    clearRect.left = rect_.left;
  }
  if (clearRect.top < rect_.top)
  {
    clearRect.top = rect_.top;
  }
  if (clearRect.right > rect_.right)
  {
    clearRect.right = rect_.right;
  }
  if (clearRect.bottom > rect_.bottom)
  {
    clearRect.bottom = rect_.bottom;
  }
  if (model.hasDirtyRect)
  {
    const short modelDirtyLeft = static_cast<short>(rect_.left + model.dirtyX);
    const short modelDirtyTop = static_cast<short>(rect_.top + model.dirtyY);
    const short modelDirtyRight = static_cast<short>(modelDirtyLeft + model.dirtyWidth);
    const short modelDirtyBottom = static_cast<short>(modelDirtyTop + model.dirtyHeight);
    if (clearRect.left < modelDirtyLeft)
    {
      clearRect.left = modelDirtyLeft;
    }
    if (clearRect.top < modelDirtyTop)
    {
      clearRect.top = modelDirtyTop;
    }
    if (clearRect.right > modelDirtyRight)
    {
      clearRect.right = modelDirtyRight;
    }
    if (clearRect.bottom > modelDirtyBottom)
    {
      clearRect.bottom = modelDirtyBottom;
    }
  }
  if (node_->props.clearBackground_ &&
      clearRect.left < clearRect.right && clearRect.top < clearRect.bottom)
  {
    EraseRect(&clearRect);
  }

  for (short i = 0; i < model.rectCount; ++i)
  {
    Rect spriteRect;
    spriteRect.left = static_cast<short>(rect_.left + model.rects[i].x);
    spriteRect.top = static_cast<short>(rect_.top + model.rects[i].y);
    spriteRect.right = static_cast<short>(spriteRect.left + model.rects[i].width);
    spriteRect.bottom = static_cast<short>(spriteRect.top + model.rects[i].height);
    if (spriteRect.right < dirtyRect.left || spriteRect.left > dirtyRect.right ||
        spriteRect.bottom < dirtyRect.top || spriteRect.top > dirtyRect.bottom)
    {
      continue;
    }
    PaintRect(&spriteRect);
  }
}

bool ToolboxRectSurfaceContext::dirtyRect(Rect &outRect) const
{
  if (!node_ || !node_->props.model_)
  {
    return false;
  }
  const loka::app::RectSurfaceModel model = node_->props.model_->get();
  if (!model.hasDirtyRect)
  {
    outRect = rect_;
    return true;
  }
  outRect.left = static_cast<short>(rect_.left + model.dirtyX);
  outRect.top = static_cast<short>(rect_.top + model.dirtyY);
  outRect.right = static_cast<short>(outRect.left + model.dirtyWidth);
  outRect.bottom = static_cast<short>(outRect.top + model.dirtyHeight);
  if (outRect.left < rect_.left)
  {
    outRect.left = rect_.left;
  }
  if (outRect.top < rect_.top)
  {
    outRect.top = rect_.top;
  }
  if (outRect.right > rect_.right)
  {
    outRect.right = rect_.right;
  }
  if (outRect.bottom > rect_.bottom)
  {
    outRect.bottom = rect_.bottom;
  }
  return outRect.left < outRect.right && outRect.top < outRect.bottom;
}
