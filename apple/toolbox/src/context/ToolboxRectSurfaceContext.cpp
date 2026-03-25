#include "context/ToolboxRectSurfaceContext.hpp"
#include "loka/core/Profiler.hpp"

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
      regionClipFrameCount_(0),
      dumpedProfileLog_(false)
{
  SetRect(&rect_, 0, 0, 0, 0);
  loka::core::ResetFuncProfile();
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
  PROFILE_FUNC();
  if (!node_ || !node_->props.model_)
  {
    return;
  }
  if (node_->props.clearBackground_)
  {
    PROFILE_SECTION_ID("rect.fullErase", 1);
    EraseRect(&rect_);
  }
  const loka::app::RectSurfaceModel model = node_->props.model_->get();
  PROFILE_SECTION_ID("rect.fullPaint", 2);
  for (short i = 0; i < model.rectCount; ++i)
  {
    Rect spriteRect = rectForSprite(model.rects[i]);
    PaintRect(&spriteRect);
  }
  rememberCurrentModel();
  noteFrame(false);
}

void ToolboxRectSurfaceContext::renderDirty(const Rect &dirtyRect)
{
  PROFILE_FUNC();
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
  bool useRegionClip = false;
  {
    PROFILE_SECTION_ID("rect.dirtyRegion", 3);
    useRegionClip = node_->props.useRegionClip_ &&
                    buildDirtyRegion(dirtyRect, model) &&
                    dirtyRgn_ != 0 &&
                    savedClipRgn_ != 0;
  }
  if (useRegionClip)
  {
    PROFILE_SECTION_ID("rect.setClip", 4);
    GetClip(savedClipRgn_);
    SetClip(dirtyRgn_);
  }
  if (node_->props.clearBackground_)
  {
    PROFILE_SECTION_ID("rect.erase", 5);
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
      }
    }
  }

  PROFILE_SECTION_ID("rect.paint", 6);
  for (short i = 0; i < model.rectCount; ++i)
  {
    Rect spriteRect = rectForSprite(model.rects[i]);
    Rect clippedRect = spriteRect;
    if (!SectRect(&clippedRect, &dirtyRect, &clippedRect))
    {
      continue;
    }
    Rect previousSpriteRect;
    if (previousRectForIndex(i, previousSpriteRect))
    {
      if (previousSpriteRect.left == spriteRect.left &&
          previousSpriteRect.top == spriteRect.top &&
          previousSpriteRect.right == spriteRect.right &&
          previousSpriteRect.bottom == spriteRect.bottom)
      {
        continue;
      }
      if ((previousSpriteRect.right - previousSpriteRect.left) ==
              (spriteRect.right - spriteRect.left) &&
          (previousSpriteRect.bottom - previousSpriteRect.top) ==
              (spriteRect.bottom - spriteRect.top))
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
    PROFILE_SECTION_ID("rect.restoreClip", 7);
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

Rect ToolboxRectSurfaceContext::rectForSprite(const loka::app::RectSprite &sprite) const
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
  if (!hasPreviousModel_ || index < 0 || index >= previousModel_.rectCount)
  {
    return false;
  }
  previousRect = rectForSprite(previousModel_.rects[index]);
  return true;
}

bool ToolboxRectSurfaceContext::findMatchingCurrentRect(const Rect &previousRect,
                                                        const loka::app::RectSurfaceModel &model,
                                                        Rect &currentRect) const
{
  bool foundAnyMatch = false;
  bool foundSizeMatch = false;
  Rect sizeMatchedRect;
  for (short i = 0; i < model.rectCount; ++i)
  {
    Rect candidateRect;
    candidateRect.left = static_cast<short>(rect_.left + model.rects[i].x);
    candidateRect.top = static_cast<short>(rect_.top + model.rects[i].y);
    candidateRect.right = static_cast<short>(candidateRect.left + model.rects[i].width);
    candidateRect.bottom = static_cast<short>(candidateRect.top + model.rects[i].height);
    Rect overlap = previousRect;
    if (!SectRect(&overlap, &candidateRect, &overlap))
    {
      continue;
    }
    if ((candidateRect.right - candidateRect.left) ==
            (previousRect.right - previousRect.left) &&
        (candidateRect.bottom - candidateRect.top) ==
            (previousRect.bottom - previousRect.top))
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

void ToolboxRectSurfaceContext::noteFrame(bool usedRegionClip)
{
  if (dumpedProfileLog_)
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
    if (!dumpedProfileLog_)
    {
      dumpProfileLog();
    }
  }
}

void ToolboxRectSurfaceContext::dumpProfileLog()
{
  struct FileStream : public loka::core::ProfileOutputStream
  {
    explicit FileStream(FILE *file) : file_(file) {}
    virtual void write(const char *data, int len)
    {
      if (!file_ || len <= 0)
      {
        return;
      }
      std::fwrite(data, 1, static_cast<size_t>(len), file_);
    }
    FILE *file_;
  };

  FILE *fp = std::fopen("FBPROF.TXT", "wb");
  if (!fp)
  {
    dumpedProfileLog_ = true;
    return;
  }
  std::fprintf(fp, "frames=%lu\r", loggedFrames_);
  FileStream out(fp);
  loka::core::DumpFuncProfileToStream(out);
  std::fclose(fp);
  dumpedProfileLog_ = true;
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
