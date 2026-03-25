#include "context/ToolboxRectSurfaceContext.hpp"
#include <stdio.h>

ToolboxRectSurfaceContext::ToolboxRectSurfaceContext(loka::app::RectSurfaceNode *node)
    : node_(node),
      boundary_(0),
      rect_(),
      previousModel_(),
      hasPreviousModel_(false),
      dirtyRgn_(NewRgn()),
      tempRgn_(NewRgn()),
      savedClipRgn_(NewRgn()),
      loggedFrames_(0),
      eraseRectCount_(0),
      paintRectCount_(0),
      eraseArea_(0),
      paintArea_(0),
      regionClipFrameCount_(0),
      dumpedFrameLog_(false)
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
    noteEraseRect(rect_);
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
    notePaintRect(spriteRect);
  }
  rememberCurrentModel();
  noteFrame(false);
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
  const bool useRegionClip = node_->props.useRegionClip_ &&
                             buildDirtyRegion(dirtyRect, model) &&
                             dirtyRgn_ != 0 &&
                             savedClipRgn_ != 0;
  if (useRegionClip)
  {
    GetClip(savedClipRgn_);
    SetClip(dirtyRgn_);
  }
  if (node_->props.clearBackground_)
  {
    if (hasPreviousModel_)
    {
      for (short i = 0; i < previousModel_.rectCount; ++i)
      {
        Rect previousSpriteRect;
        previousSpriteRect.left = static_cast<short>(rect_.left + previousModel_.rects[i].x);
        previousSpriteRect.top = static_cast<short>(rect_.top + previousModel_.rects[i].y);
        previousSpriteRect.right = static_cast<short>(previousSpriteRect.left + previousModel_.rects[i].width);
        previousSpriteRect.bottom = static_cast<short>(previousSpriteRect.top + previousModel_.rects[i].height);
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
        noteEraseRect(clearRect);
      }
    }
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
    PaintRect(&spriteRect);
    notePaintRect(spriteRect);
  }
  if (useRegionClip)
  {
    SetClip(savedClipRgn_);
  }
  rememberCurrentModel();
  noteFrame(useRegionClip);
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

bool ToolboxRectSurfaceContext::findMatchingCurrentRect(const Rect &previousRect,
                                                        const loka::app::RectSurfaceModel &model,
                                                        Rect &currentRect) const
{
  for (short i = 0; i < model.rectCount; ++i)
  {
    currentRect.left = static_cast<short>(rect_.left + model.rects[i].x);
    currentRect.top = static_cast<short>(rect_.top + model.rects[i].y);
    currentRect.right = static_cast<short>(currentRect.left + model.rects[i].width);
    currentRect.bottom = static_cast<short>(currentRect.top + model.rects[i].height);
    Rect overlap = previousRect;
    if (!SectRect(&overlap, &currentRect, &overlap))
    {
      continue;
    }
    return true;
  }
  return false;
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
  noteEraseRect(eraseRect);
}

bool ToolboxRectSurfaceContext::currentModelContainsRect(const Rect &rect,
                                                         const loka::app::RectSurfaceModel &model) const
{
  for (short i = 0; i < model.rectCount; ++i)
  {
    Rect currentRect;
    currentRect.left = static_cast<short>(rect_.left + model.rects[i].x);
    currentRect.top = static_cast<short>(rect_.top + model.rects[i].y);
    currentRect.right = static_cast<short>(currentRect.left + model.rects[i].width);
    currentRect.bottom = static_cast<short>(currentRect.top + model.rects[i].height);
    if (currentRect.left == rect.left &&
        currentRect.top == rect.top &&
        currentRect.right == rect.right &&
        currentRect.bottom == rect.bottom)
    {
      return true;
    }
  }
  return false;
}

void ToolboxRectSurfaceContext::noteEraseRect(const Rect &rect)
{
  const long width = static_cast<long>(rect.right) - static_cast<long>(rect.left);
  const long height = static_cast<long>(rect.bottom) - static_cast<long>(rect.top);
  if (width <= 0 || height <= 0)
  {
    return;
  }
  ++eraseRectCount_;
  eraseArea_ += static_cast<unsigned long>(width * height);
}

void ToolboxRectSurfaceContext::notePaintRect(const Rect &rect)
{
  const long width = static_cast<long>(rect.right) - static_cast<long>(rect.left);
  const long height = static_cast<long>(rect.bottom) - static_cast<long>(rect.top);
  if (width <= 0 || height <= 0)
  {
    return;
  }
  ++paintRectCount_;
  paintArea_ += static_cast<unsigned long>(width * height);
}

void ToolboxRectSurfaceContext::noteFrame(bool usedRegionClip)
{
  if (dumpedFrameLog_)
  {
    return;
  }
  ++loggedFrames_;
  if (usedRegionClip)
  {
    ++regionClipFrameCount_;
  }
  if (loggedFrames_ >= 30UL)
  {
    dumpFrameLog();
  }
}

void ToolboxRectSurfaceContext::dumpFrameLog()
{
  FILE *fp = std::fopen("FBDRAW.TXT", "wb");
  if (!fp)
  {
    dumpedFrameLog_ = true;
    return;
  }
  std::fprintf(fp, "frames=%lu\n", loggedFrames_);
  std::fprintf(fp, "erase_rect_count=%ld\n", eraseRectCount_);
  std::fprintf(fp, "paint_rect_count=%ld\n", paintRectCount_);
  std::fprintf(fp, "erase_area=%lu\n", eraseArea_);
  std::fprintf(fp, "paint_area=%lu\n", paintArea_);
  std::fprintf(fp, "region_clip_frames=%ld\n", regionClipFrameCount_);
  if (loggedFrames_ > 0UL)
  {
    std::fprintf(fp, "avg_erase_rects=%.2f\n", static_cast<double>(eraseRectCount_) / static_cast<double>(loggedFrames_));
    std::fprintf(fp, "avg_paint_rects=%.2f\n", static_cast<double>(paintRectCount_) / static_cast<double>(loggedFrames_));
    std::fprintf(fp, "avg_erase_area=%.2f\n", static_cast<double>(eraseArea_) / static_cast<double>(loggedFrames_));
    std::fprintf(fp, "avg_paint_area=%.2f\n", static_cast<double>(paintArea_) / static_cast<double>(loggedFrames_));
  }
  std::fclose(fp);
  dumpedFrameLog_ = true;
}

bool ToolboxRectSurfaceContext::buildDirtyRegion(const Rect &dirtyRect,
                                                 const loka::app::RectSurfaceModel &model)
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
