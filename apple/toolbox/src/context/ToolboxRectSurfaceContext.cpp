#include "context/ToolboxRectSurfaceContext.hpp"
#include "ToolboxScenePlatformController.hpp"
#include "context/RectSurfaceRepaintPlan.hpp"

void EnsureToolboxRectSurfaceContext(loka::app::RectSurfaceNode *node, ToolboxScenePlatformController *controller)
{
  if (!node)
  {
    return;
  }
  ToolboxRectSurfaceContext *ctx = static_cast<ToolboxRectSurfaceContext *>(node->getContext());
  if (!ctx)
  {
    ctx = new ToolboxRectSurfaceContext(node, controller);
    node->setContext(ctx);
    if (ctx && controller)
    {
      controller->requestStructurePresent();
    }
  }
}

ToolboxRectSurfaceContext::ToolboxRectSurfaceContext(loka::app::RectSurfaceNode *node,
                                                     ToolboxScenePlatformController *controller)
    : ToolboxProjectedNodeContext(controller),
      node_(node),
      rect_(),
      previousModel_(),
      hasPreviousModel_(false),
      dirtyRgn_(NewRgn()),
      tempRgn_(NewRgn()),
      savedClipRgn_(NewRgn())
{
  SetRect(&rect_, 0, 0, 0, 0);
}

void ToolboxRectSurfaceContext::onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                                              loka::app::scene::NodeLifecycleFact next)
{
  // Detached or retired, the surface is no longer placed: its pending seat
  // rows go first, while the controller back-pointer is still intact (the
  // base clears it on RETIRED).
  if (next != loka::app::scene::NODE_FACT_ATTACHED && this->controller())
  {
    this->controller()->cancelRectSurfaceExtent(node_);
  }
  ToolboxProjectedNodeContext::onFactChanged(previous, next);
}

ToolboxRectSurfaceContext::~ToolboxRectSurfaceContext()
{
  if (dirtyRgn_)
  {
    DisposeRgn(dirtyRgn_);
    dirtyRgn_ = 0;
  }
  if (tempRgn_)
  {
    DisposeRgn(tempRgn_);
    tempRgn_ = 0;
  }
  if (savedClipRgn_)
  {
    DisposeRgn(savedClipRgn_);
    savedClipRgn_ = 0;
  }
}

short ToolboxRectSurfaceContext::layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state)
{
  if (!node_)
  {
    return 0;
  }
  rect_.left = state.x;
  rect_.top = static_cast<short>(state.y);
  rect_.right = static_cast<short>(state.x + state.width);
  rect_.bottom = static_cast<short>(state.y + state.height);
  state.y = static_cast<short>(rect_.bottom + state.spacing);
  return state.width;
}

void ToolboxRectSurfaceContext::render(loka::app::scene::IPlatformController *)
{
  if (!node_ || !node_->props.model_)
  {
    return;
  }
  // A walk can run under a clip that excludes this surface entirely (the #412
  // dirty escalation clips render() to the damaged rect). Painting would be a
  // no-op there, but rememberCurrentModel() would still overwrite the previous
  // sprite positions, and the surface's own pending dirty flush then loses the
  // old rects it must erase. If nothing here can be painted, do not claim a
  // paint happened. The snapshot stays older, which only widens a later dirty
  // region -- overpaint, never stale pixels.
  if (tempRgn_)
  {
    GetClip(tempRgn_);
    const Rect clipBounds = (**tempRgn_).rgnBBox;
    if (clipBounds.right < rect_.left || clipBounds.left > rect_.right || clipBounds.bottom < rect_.top
        || clipBounds.top > rect_.bottom)
    {
      return;
    }
  }
  if (node_->props.clearBackground_)
  {
    EraseRect(&rect_);
  }
  const loka::app::RectSurfaceModel model = node_->props.model_->get();
  for (short i = 0; i < model.rectCount; ++i)
  {
    Rect spriteRect = rectForSprite(model.rects[i]);
    PaintRect(&spriteRect);
  }
  rememberCurrentModel();
}

void ToolboxRectSurfaceContext::renderDirty(const Rect &dirtyRect)
{
  if (!node_ || !node_->props.model_)
  {
    return;
  }
  if (dirtyRect.right < rect_.left || dirtyRect.left > rect_.right || dirtyRect.bottom < rect_.top
      || dirtyRect.top > rect_.bottom)
  {
    return;
  }

  const loka::app::RectSurfaceModel model = node_->props.model_->get();
  bool useRegionClip = false;
  useRegionClip =
      node_->props.useRegionClip_ && buildDirtyRegion(dirtyRect, model) && dirtyRgn_ != 0 && savedClipRgn_ != 0;
  if (useRegionClip)
  {
    GetClip(savedClipRgn_);
    SetClip(dirtyRgn_);
  }
  const loka::toolbox::RectSurfaceRepaintPlan plan(
      this->hasPreviousModel_ ? &this->previousModel_ : 0, model,
      loka::core::Frame(this->rect_.left, this->rect_.top,
                        this->rect_.right - this->rect_.left, this->rect_.bottom - this->rect_.top),
      loka::core::Frame(dirtyRect.left, dirtyRect.top,
                        dirtyRect.right - dirtyRect.left, dirtyRect.bottom - dirtyRect.top),
      this->node_->props.clearBackground_);
  for (short i = 0; i < plan.eraseCount(); ++i)
  {
    const loka::core::Frame &frame = plan.eraseRect(i);
    Rect rect;
    SetRect(&rect, frame.x, frame.y, frame.x + frame.width, frame.y + frame.height);
    EraseRect(&rect);
  }
  for (short i = 0; i < plan.paintCount(); ++i)
  {
    const loka::core::Frame &frame = plan.paintRect(i);
    Rect rect;
    SetRect(&rect, frame.x, frame.y, frame.x + frame.width, frame.y + frame.height);
    PaintRect(&rect);
  }
  if (useRegionClip)
  {
    SetClip(savedClipRgn_);
  }
  rememberCurrentModel();
}

bool ToolboxRectSurfaceContext::dirtyRect(Rect &outRect) const
{
  if (!node_ || !node_->props.model_)
  {
    return false;
  }
  const loka::app::RectSurfaceModel model = node_->props.model_->get();
  bool hasBounds = false;
  if (model.rectCount > 0)
  {
    outRect.left = static_cast<short>(rect_.left + model.rects[0].x);
    outRect.top = static_cast<short>(rect_.top + model.rects[0].y);
    outRect.right = static_cast<short>(outRect.left + model.rects[0].width);
    outRect.bottom = static_cast<short>(outRect.top + model.rects[0].height);
    hasBounds = true;
  }
  for (short i = 1; i < model.rectCount; ++i)
  {
    const short left = static_cast<short>(rect_.left + model.rects[i].x);
    const short top = static_cast<short>(rect_.top + model.rects[i].y);
    const short right = static_cast<short>(left + model.rects[i].width);
    const short bottom = static_cast<short>(top + model.rects[i].height);
    if (!hasBounds)
    {
      outRect.left = left;
      outRect.top = top;
      outRect.right = right;
      outRect.bottom = bottom;
      hasBounds = true;
      continue;
    }
    if (left < outRect.left)
    {
      outRect.left = left;
    }
    if (top < outRect.top)
    {
      outRect.top = top;
    }
    if (right > outRect.right)
    {
      outRect.right = right;
    }
    if (bottom > outRect.bottom)
    {
      outRect.bottom = bottom;
    }
  }
  if (hasPreviousModel_)
  {
    for (short i = 0; i < previousModel_.rectCount; ++i)
    {
      const short left = static_cast<short>(rect_.left + previousModel_.rects[i].x);
      const short top = static_cast<short>(rect_.top + previousModel_.rects[i].y);
      const short right = static_cast<short>(left + previousModel_.rects[i].width);
      const short bottom = static_cast<short>(top + previousModel_.rects[i].height);
      if (!hasBounds)
      {
        outRect.left = left;
        outRect.top = top;
        outRect.right = right;
        outRect.bottom = bottom;
        hasBounds = true;
        continue;
      }
      if (left < outRect.left)
      {
        outRect.left = left;
      }
      if (top < outRect.top)
      {
        outRect.top = top;
      }
      if (right > outRect.right)
      {
        outRect.right = right;
      }
      if (bottom > outRect.bottom)
      {
        outRect.bottom = bottom;
      }
    }
  }
  if (!hasBounds)
  {
    outRect = rect_;
    return true;
  }
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

void ToolboxRectSurfaceContext::rememberCurrentModel()
{
  if (!node_ || !node_->props.model_)
  {
    hasPreviousModel_ = false;
    previousModel_ = loka::app::RectSurfaceModel();
    return;
  }
  previousModel_ = node_->props.model_->get();
  hasPreviousModel_ = true;
}

Rect ToolboxRectSurfaceContext::rectForSprite(const loka::app::RectSprite &sprite) const
{
  Rect rect;
  rect.left = static_cast<short>(rect_.left + sprite.x);
  rect.top = static_cast<short>(rect_.top + sprite.y);
  rect.right = static_cast<short>(rect.left + sprite.width);
  rect.bottom = static_cast<short>(rect.top + sprite.height);
  return rect;
}

bool ToolboxRectSurfaceContext::buildDirtyRegion(const Rect &dirtyRect, const loka::app::RectSurfaceModel &model)
{
  if (!dirtyRgn_ || !tempRgn_)
  {
    return false;
  }
  SetEmptyRgn(dirtyRgn_);
  unionSpriteRectsIntoRegion(model, dirtyRect);
  if (hasPreviousModel_)
  {
    unionSpriteRectsIntoRegion(previousModel_, dirtyRect);
  }
  Rect bounds = (**dirtyRgn_).rgnBBox;
  return bounds.left < bounds.right && bounds.top < bounds.bottom;
}

void ToolboxRectSurfaceContext::unionSpriteRectsIntoRegion(const loka::app::RectSurfaceModel &model,
                                                           const Rect &dirtyRect)
{
  if (!dirtyRgn_ || !tempRgn_)
  {
    return;
  }
  for (short i = 0; i < model.rectCount; ++i)
  {
    Rect spriteRect;
    spriteRect.left = static_cast<short>(rect_.left + model.rects[i].x);
    spriteRect.top = static_cast<short>(rect_.top + model.rects[i].y);
    spriteRect.right = static_cast<short>(spriteRect.left + model.rects[i].width);
    spriteRect.bottom = static_cast<short>(spriteRect.top + model.rects[i].height);
    Rect clippedRect = spriteRect;
    if (!SectRect(&clippedRect, &dirtyRect, &clippedRect))
    {
      continue;
    }
    RectRgn(tempRgn_, &clippedRect);
    UnionRgn(dirtyRgn_, tempRgn_, dirtyRgn_);
  }
}
