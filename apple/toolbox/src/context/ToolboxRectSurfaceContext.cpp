#include "context/ToolboxRectSurfaceContext.hpp"
#include "ToolboxScenePlatformController.hpp"

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
  for (short i = 0; i < model.spriteCount; ++i)
  {
    const loka::app::RectSurfaceSprite &sprite = model.sprites[i];
    switch (sprite.kind())
    {
    case loka::app::RectSurfaceSprite::KIND_RECT:
    {
      Rect spriteRect = rectForSprite(sprite);
      PaintRect(&spriteRect);
      break;
    }
    case loka::app::RectSurfaceSprite::KIND_IMAGE:
      break;
    }
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
  if (node_->props.clearBackground_)
  {
    if (hasPreviousModel_)
    {
      for (short i = 0; i < previousModel_.spriteCount; ++i)
      {
        Rect previousSpriteRect;
        previousSpriteRect.left = static_cast<short>(rect_.left + previousModel_.sprites[i].x);
        previousSpriteRect.top = static_cast<short>(rect_.top + previousModel_.sprites[i].y);
        previousSpriteRect.right = static_cast<short>(previousSpriteRect.left + previousModel_.sprites[i].width);
        previousSpriteRect.bottom = static_cast<short>(previousSpriteRect.top + previousModel_.sprites[i].height);
        if (currentModelContainsRect(previousSpriteRect, model))
        {
          continue;
        }
        Rect matchingCurrentRect;
        if (findMatchingCurrentRect(previousSpriteRect, model, matchingCurrentRect))
        {
          erasePreviousMinusCurrent(previousSpriteRect, matchingCurrentRect, dirtyRect);
        }
        else
        {
          eraseRectIfVisible(previousSpriteRect, dirtyRect);
        }
      }
    }
    else
    {
      Rect clearRect = rect_;
      if (SectRect(&clearRect, &dirtyRect, &clearRect))
      {
        EraseRect(&clearRect);
      }
    }
  }

  for (short i = 0; i < model.spriteCount; ++i)
  {
    const loka::app::RectSurfaceSprite &sprite = model.sprites[i];
    switch (sprite.kind())
    {
    case loka::app::RectSurfaceSprite::KIND_RECT:
      break;
    case loka::app::RectSurfaceSprite::KIND_IMAGE:
      continue;
    }
    Rect spriteRect = rectForSprite(sprite);
    Rect clippedRect = spriteRect;
    if (!SectRect(&clippedRect, &dirtyRect, &clippedRect))
    {
      continue;
    }
    Rect previousSpriteRect;
    if (previousRectForIndex(i, previousSpriteRect))
    {
      if (previousSpriteRect.left == spriteRect.left && previousSpriteRect.top == spriteRect.top
          && previousSpriteRect.right == spriteRect.right && previousSpriteRect.bottom == spriteRect.bottom)
      {
        continue;
      }
      if ((previousSpriteRect.right - previousSpriteRect.left) == (spriteRect.right - spriteRect.left)
          && (previousSpriteRect.bottom - previousSpriteRect.top) == (spriteRect.bottom - spriteRect.top))
      {
        paintCurrentMinusPrevious(spriteRect, previousSpriteRect, dirtyRect);
        continue;
      }
    }
    else if (hasPreviousModel_ && currentModelContainsRect(spriteRect, previousModel_))
    {
      continue;
    }
    paintRectIfVisible(spriteRect, dirtyRect);
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
  if (model.spriteCount > 0)
  {
    outRect.left = static_cast<short>(rect_.left + model.sprites[0].x);
    outRect.top = static_cast<short>(rect_.top + model.sprites[0].y);
    outRect.right = static_cast<short>(outRect.left + model.sprites[0].width);
    outRect.bottom = static_cast<short>(outRect.top + model.sprites[0].height);
    hasBounds = true;
  }
  for (short i = 1; i < model.spriteCount; ++i)
  {
    const short left = static_cast<short>(rect_.left + model.sprites[i].x);
    const short top = static_cast<short>(rect_.top + model.sprites[i].y);
    const short right = static_cast<short>(left + model.sprites[i].width);
    const short bottom = static_cast<short>(top + model.sprites[i].height);
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
    for (short i = 0; i < previousModel_.spriteCount; ++i)
    {
      const short left = static_cast<short>(rect_.left + previousModel_.sprites[i].x);
      const short top = static_cast<short>(rect_.top + previousModel_.sprites[i].y);
      const short right = static_cast<short>(left + previousModel_.sprites[i].width);
      const short bottom = static_cast<short>(top + previousModel_.sprites[i].height);
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

Rect ToolboxRectSurfaceContext::rectForSprite(const loka::app::RectSurfaceSprite &sprite) const
{
  Rect rect;
  rect.left = static_cast<short>(rect_.left + sprite.x);
  rect.top = static_cast<short>(rect_.top + sprite.y);
  rect.right = static_cast<short>(rect.left + sprite.width);
  rect.bottom = static_cast<short>(rect.top + sprite.height);
  return rect;
}

bool ToolboxRectSurfaceContext::previousRectForIndex(short index, Rect &previousRect) const
{
  if (!hasPreviousModel_ || index < 0 || index >= previousModel_.spriteCount)
  {
    return false;
  }
  previousRect = rectForSprite(previousModel_.sprites[index]);
  return true;
}

bool ToolboxRectSurfaceContext::findMatchingCurrentRect(const Rect &previousRect,
                                                        const loka::app::RectSurfaceModel &model,
                                                        Rect &currentRect) const
{
  bool foundAnyMatch = false;
  bool foundSizeMatch = false;
  Rect sizeMatchedRect;
  for (short i = 0; i < model.spriteCount; ++i)
  {
    Rect candidateRect;
    candidateRect.left = static_cast<short>(rect_.left + model.sprites[i].x);
    candidateRect.top = static_cast<short>(rect_.top + model.sprites[i].y);
    candidateRect.right = static_cast<short>(candidateRect.left + model.sprites[i].width);
    candidateRect.bottom = static_cast<short>(candidateRect.top + model.sprites[i].height);
    Rect overlap = previousRect;
    if (!SectRect(&overlap, &candidateRect, &overlap))
    {
      continue;
    }
    if ((candidateRect.right - candidateRect.left) == (previousRect.right - previousRect.left)
        && (candidateRect.bottom - candidateRect.top) == (previousRect.bottom - previousRect.top))
    {
      sizeMatchedRect = candidateRect;
      foundSizeMatch = true;
      break;
    }
    currentRect = candidateRect;
    foundAnyMatch = true;
  }
  if (foundSizeMatch)
  {
    currentRect = sizeMatchedRect;
    return true;
  }
  return foundAnyMatch;
}

void ToolboxRectSurfaceContext::paintCurrentMinusPrevious(const Rect &currentRect,
                                                          const Rect &previousRect,
                                                          const Rect &dirtyRect)
{
  Rect overlap = currentRect;
  if (!SectRect(&overlap, &previousRect, &overlap))
  {
    paintRectIfVisible(currentRect, dirtyRect);
    return;
  }

  Rect topRect = currentRect;
  topRect.bottom = overlap.top;
  paintRectIfVisible(topRect, dirtyRect);

  Rect bottomRect = currentRect;
  bottomRect.top = overlap.bottom;
  paintRectIfVisible(bottomRect, dirtyRect);

  Rect leftRect = currentRect;
  leftRect.top = overlap.top;
  leftRect.bottom = overlap.bottom;
  leftRect.right = overlap.left;
  paintRectIfVisible(leftRect, dirtyRect);

  Rect rightRect = currentRect;
  rightRect.top = overlap.top;
  rightRect.bottom = overlap.bottom;
  rightRect.left = overlap.right;
  paintRectIfVisible(rightRect, dirtyRect);
}

void ToolboxRectSurfaceContext::erasePreviousMinusCurrent(const Rect &previousRect,
                                                          const Rect &currentRect,
                                                          const Rect &dirtyRect)
{
  Rect overlap = previousRect;
  if (!SectRect(&overlap, &currentRect, &overlap))
  {
    eraseRectIfVisible(previousRect, dirtyRect);
    return;
  }

  Rect topRect = previousRect;
  topRect.bottom = overlap.top;
  eraseRectIfVisible(topRect, dirtyRect);

  Rect bottomRect = previousRect;
  bottomRect.top = overlap.bottom;
  eraseRectIfVisible(bottomRect, dirtyRect);

  Rect leftRect = previousRect;
  leftRect.top = overlap.top;
  leftRect.bottom = overlap.bottom;
  leftRect.right = overlap.left;
  eraseRectIfVisible(leftRect, dirtyRect);

  Rect rightRect = previousRect;
  rightRect.top = overlap.top;
  rightRect.bottom = overlap.bottom;
  rightRect.left = overlap.right;
  eraseRectIfVisible(rightRect, dirtyRect);
}

void ToolboxRectSurfaceContext::eraseRectIfVisible(const Rect &rect, const Rect &dirtyRect)
{
  Rect eraseRect = rect;
  if (!SectRect(&eraseRect, &dirtyRect, &eraseRect))
  {
    return;
  }
  if (eraseRect.left >= eraseRect.right || eraseRect.top >= eraseRect.bottom)
  {
    return;
  }
  EraseRect(&eraseRect);
}

void ToolboxRectSurfaceContext::paintRectIfVisible(const Rect &rect, const Rect &dirtyRect)
{
  Rect paintRect = rect;
  if (!SectRect(&paintRect, &dirtyRect, &paintRect))
  {
    return;
  }
  if (paintRect.left >= paintRect.right || paintRect.top >= paintRect.bottom)
  {
    return;
  }
  PaintRect(&paintRect);
}

bool ToolboxRectSurfaceContext::currentModelContainsRect(const Rect &rect,
                                                         const loka::app::RectSurfaceModel &model) const
{
  for (short i = 0; i < model.spriteCount; ++i)
  {
    Rect currentRect;
    currentRect.left = static_cast<short>(rect_.left + model.sprites[i].x);
    currentRect.top = static_cast<short>(rect_.top + model.sprites[i].y);
    currentRect.right = static_cast<short>(currentRect.left + model.sprites[i].width);
    currentRect.bottom = static_cast<short>(currentRect.top + model.sprites[i].height);
    if (currentRect.left == rect.left && currentRect.top == rect.top && currentRect.right == rect.right
        && currentRect.bottom == rect.bottom)
    {
      return true;
    }
  }
  return false;
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
  for (short i = 0; i < model.spriteCount; ++i)
  {
    Rect spriteRect;
    spriteRect.left = static_cast<short>(rect_.left + model.sprites[i].x);
    spriteRect.top = static_cast<short>(rect_.top + model.sprites[i].y);
    spriteRect.right = static_cast<short>(spriteRect.left + model.sprites[i].width);
    spriteRect.bottom = static_cast<short>(spriteRect.top + model.sprites[i].height);
    Rect clippedRect = spriteRect;
    if (!SectRect(&clippedRect, &dirtyRect, &clippedRect))
    {
      continue;
    }
    RectRgn(tempRgn_, &clippedRect);
    UnionRgn(dirtyRgn_, tempRgn_, dirtyRgn_);
  }
}
