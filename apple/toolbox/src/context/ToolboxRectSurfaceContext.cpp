#include "context/ToolboxRectSurfaceContext.hpp"
#include "ToolboxScenePlatformController.hpp"
#include "ToolboxNativeImage.hpp"
#include <cassert>

namespace
{
  static bool IsColorPort(GrafPtr port)
  {
    if (!port)
    {
      return false;
    }
    const short firstWord = *reinterpret_cast<short *>(port);
    return (firstWord & static_cast<short>(0xC000)) != 0;
  }

  static const BitMap *PortBitsForCopyMask(GrafPtr port)
  {
    if (!port)
    {
      return 0;
    }
    if (IsColorPort(port))
    {
      PixMapHandle portPixMap = ((CGrafPtr)port)->portPixMap;
      if (!portPixMap || !*portPixMap)
      {
        return 0;
      }
      return reinterpret_cast<const BitMap *>(*portPixMap);
    }
    return &port->portBits;
  }

} // namespace

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
  // no-op there, but committing the applied snapshot would still overwrite
  // the previous sprite positions, and the surface's own pending dirty flush then loses the
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
  const loka::app::RectSurfacePaintList paintList(model);
  loka::app::RectSurfacePaintResult paintResult = loka::app::RECT_SURFACE_PAINT_SUCCEEDED;
  for (short i = 0; i < paintList.count(); ++i)
  {
    const loka::app::RectSurfaceSprite &sprite = *paintList.querySprite(i);
    switch (sprite.kind())
    {
    case loka::app::RectSurfaceSprite::KIND_RECT:
    {
      Rect spriteRect = rectForSprite(sprite);
      PaintRect(&spriteRect);
      break;
    }
    case loka::app::RectSurfaceSprite::KIND_IMAGE:
    {
      const Rect spriteRect = rectForSprite(sprite);
      if (paintImage(sprite, spriteRect, 0) == loka::app::RECT_SURFACE_PAINT_REFUSED)
      {
        paintResult = loka::app::RECT_SURFACE_PAINT_REFUSED;
      }
      break;
    }
    }
  }
  finishPaint(paintResult, model);
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
  const loka::app::RectSurfacePaintList paintList(model);
  loka::app::RectSurfacePaintResult paintResult = loka::app::RECT_SURFACE_PAINT_SUCCEEDED;
  const bool replayPaintOrder = modelContainsImage(model) || (hasPreviousModel_ && modelContainsImage(previousModel_));
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
    if (replayPaintOrder)
    {
      Rect clearRect = rect_;
      if (SectRect(&clearRect, &dirtyRect, &clearRect))
      {
        EraseRect(&clearRect);
      }
    }
    else if (hasPreviousModel_)
    {
      for (short i = 0; i < previousModel_.spriteCount(); ++i)
      {
        Rect previousSpriteRect;
        previousSpriteRect.left = static_cast<short>(rect_.left + previousModel_.sprite(i).x);
        previousSpriteRect.top = static_cast<short>(rect_.top + previousModel_.sprite(i).y);
        previousSpriteRect.right = static_cast<short>(previousSpriteRect.left + previousModel_.sprite(i).width);
        previousSpriteRect.bottom = static_cast<short>(previousSpriteRect.top + previousModel_.sprite(i).height);
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

  for (short i = 0; i < paintList.count(); ++i)
  {
    const loka::app::RectSurfaceSprite &sprite = *paintList.querySprite(i);
    Rect spriteRect = rectForSprite(sprite);
    Rect clippedRect = spriteRect;
    if (!SectRect(&clippedRect, &dirtyRect, &clippedRect))
    {
      continue;
    }
    if (replayPaintOrder)
    {
      switch (sprite.kind())
      {
      case loka::app::RectSurfaceSprite::KIND_RECT:
        paintRectIfVisible(spriteRect, dirtyRect);
        break;
      case loka::app::RectSurfaceSprite::KIND_IMAGE:
        if (paintImage(sprite, spriteRect, &dirtyRect) == loka::app::RECT_SURFACE_PAINT_REFUSED)
        {
          paintResult = loka::app::RECT_SURFACE_PAINT_REFUSED;
        }
        break;
      }
      continue;
    }
    Rect previousSpriteRect;
    if (previousRectForIndex(i, previousSpriteRect))
    {
      if (previousSpriteRect.left == spriteRect.left && previousSpriteRect.top == spriteRect.top
          && previousSpriteRect.right == spriteRect.right && previousSpriteRect.bottom == spriteRect.bottom
          && loka::app::RectSurfaceSpriteRequiresRepaint(sprite, previousModel_.sprite(i)) == false)
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
  finishPaint(paintResult, model);
}

void ToolboxRectSurfaceContext::finishPaint(loka::app::RectSurfacePaintResult result,
                                            const loka::app::RectSurfaceModel &requestedModel)
{
  loka::app::FinishRectSurfacePaint(result, requestedModel, previousModel_);
  switch (result)
  {
  case loka::app::RECT_SURFACE_PAINT_SUCCEEDED:
    hasPreviousModel_ = true;
    break;
  case loka::app::RECT_SURFACE_PAINT_REFUSED:
    if (this->controller())
    {
      this->controller()->requestRectSurfacePaintRetry(rect_);
    }
    break;
  }
}

bool ToolboxRectSurfaceContext::dirtyRect(Rect &outRect) const
{
  if (!node_ || !node_->props.model_)
  {
    return false;
  }
  const loka::app::RectSurfaceModel model = node_->props.model_->get();
  bool hasBounds = false;
  if (model.spriteCount() > 0)
  {
    outRect.left = static_cast<short>(rect_.left + model.sprite(0).x);
    outRect.top = static_cast<short>(rect_.top + model.sprite(0).y);
    outRect.right = static_cast<short>(outRect.left + model.sprite(0).width);
    outRect.bottom = static_cast<short>(outRect.top + model.sprite(0).height);
    hasBounds = true;
  }
  for (short i = 1; i < model.spriteCount(); ++i)
  {
    const short left = static_cast<short>(rect_.left + model.sprite(i).x);
    const short top = static_cast<short>(rect_.top + model.sprite(i).y);
    const short right = static_cast<short>(left + model.sprite(i).width);
    const short bottom = static_cast<short>(top + model.sprite(i).height);
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
    for (short i = 0; i < previousModel_.spriteCount(); ++i)
    {
      const short left = static_cast<short>(rect_.left + previousModel_.sprite(i).x);
      const short top = static_cast<short>(rect_.top + previousModel_.sprite(i).y);
      const short right = static_cast<short>(left + previousModel_.sprite(i).width);
      const short bottom = static_cast<short>(top + previousModel_.sprite(i).height);
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
  if (!hasPreviousModel_ || index < 0 || index >= previousModel_.spriteCount())
  {
    return false;
  }
  previousRect = rectForSprite(previousModel_.sprite(index));
  return true;
}

bool ToolboxRectSurfaceContext::findMatchingCurrentRect(const Rect &previousRect,
                                                        const loka::app::RectSurfaceModel &model,
                                                        Rect &currentRect) const
{
  bool foundAnyMatch = false;
  bool foundSizeMatch = false;
  Rect sizeMatchedRect;
  for (short i = 0; i < model.spriteCount(); ++i)
  {
    Rect candidateRect;
    candidateRect.left = static_cast<short>(rect_.left + model.sprite(i).x);
    candidateRect.top = static_cast<short>(rect_.top + model.sprite(i).y);
    candidateRect.right = static_cast<short>(candidateRect.left + model.sprite(i).width);
    candidateRect.bottom = static_cast<short>(candidateRect.top + model.sprite(i).height);
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

loka::app::RectSurfacePaintResult
ToolboxRectSurfaceContext::paintImage(const loka::app::RectSurfaceSprite &sprite,
                                      const Rect &destinationRect,
                                      const Rect *dirtyRect)
{
  loka::core::resource::Image image;
  if (!sprite.queryImage(image) || !image.isValid() || sprite.width <= 0 || sprite.height <= 0)
  {
    return loka::app::RECT_SURFACE_PAINT_SUCCEEDED;
  }
  // A handle that is not a Toolbox native image record is a construction
  // error, not resource pressure: refusing it would request the same
  // rectangle on every flush forever, so it is skipped and reported by assert
  // (the Win32 non-HBITMAP and macOS non-NSImage shapes).
  if (!loka::toolbox::TryGetToolboxNativeImage(image))
  {
    assert(!"RectSurface image sprite handle is not a Toolbox native image");
    return loka::app::RECT_SURFACE_PAINT_SUCCEEDED;
  }
  // Only the lazy one-bit mask build can fail transiently (NewPtrClear).
  const BitMap *binaryMask = loka::toolbox::PrepareToolboxBinaryMask(image);
  if (!binaryMask)
  {
    return loka::app::RECT_SURFACE_PAINT_REFUSED;
  }
  // A bounded dirty replay paints only the dirty part of the sprite. The
  // sprite is copied 1:1, so the clip is plain rect arithmetic on the
  // CopyMask rects (no clip region to allocate, which could refuse and would
  // otherwise silently paint the whole sprite over later siblings).
  Rect paintRect = destinationRect;
  if (dirtyRect && !SectRect(&paintRect, dirtyRect, &paintRect))
  {
    return loka::app::RECT_SURFACE_PAINT_SUCCEEDED;
  }
  Rect sourceRect;
  SetRect(&sourceRect,
          static_cast<short>(paintRect.left - destinationRect.left),
          static_cast<short>(paintRect.top - destinationRect.top),
          static_cast<short>(paintRect.right - destinationRect.left),
          static_cast<short>(paintRect.bottom - destinationRect.top));
  GrafPtr destinationPort = 0;
  GetPort(&destinationPort);
  if (destinationPort)
  {
    const BitMap *destinationBits = PortBitsForCopyMask(destinationPort);
    if (destinationBits)
    {
      // The Universal Interfaces declare CopyMask with non-const BitMap pointers.
      CopyMask(const_cast<BitMap *>(binaryMask),
               const_cast<BitMap *>(binaryMask),
               const_cast<BitMap *>(destinationBits),
               &sourceRect,
               &sourceRect,
               &paintRect);
    }
  }
  return loka::app::RECT_SURFACE_PAINT_SUCCEEDED;
}

bool ToolboxRectSurfaceContext::currentModelContainsRect(const Rect &rect,
                                                         const loka::app::RectSurfaceModel &model) const
{
  for (short i = 0; i < model.spriteCount(); ++i)
  {
    Rect currentRect;
    currentRect.left = static_cast<short>(rect_.left + model.sprite(i).x);
    currentRect.top = static_cast<short>(rect_.top + model.sprite(i).y);
    currentRect.right = static_cast<short>(currentRect.left + model.sprite(i).width);
    currentRect.bottom = static_cast<short>(currentRect.top + model.sprite(i).height);
    if (currentRect.left == rect.left && currentRect.top == rect.top && currentRect.right == rect.right
        && currentRect.bottom == rect.bottom)
    {
      return true;
    }
  }
  return false;
}

bool ToolboxRectSurfaceContext::modelContainsImage(const loka::app::RectSurfaceModel &model) const
{
  const loka::app::RectSurfacePaintList paintList(model);
  for (short i = 0; i < paintList.count(); ++i)
  {
    if (paintList.querySprite(i)->kind() == loka::app::RectSurfaceSprite::KIND_IMAGE)
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
  for (short i = 0; i < model.spriteCount(); ++i)
  {
    Rect spriteRect;
    spriteRect.left = static_cast<short>(rect_.left + model.sprite(i).x);
    spriteRect.top = static_cast<short>(rect_.top + model.sprite(i).y);
    spriteRect.right = static_cast<short>(spriteRect.left + model.sprite(i).width);
    spriteRect.bottom = static_cast<short>(spriteRect.top + model.sprite(i).height);
    Rect clippedRect = spriteRect;
    if (!SectRect(&clippedRect, &dirtyRect, &clippedRect))
    {
      continue;
    }
    RectRgn(tempRgn_, &clippedRect);
    UnionRgn(dirtyRgn_, tempRgn_, dirtyRgn_);
  }
}
