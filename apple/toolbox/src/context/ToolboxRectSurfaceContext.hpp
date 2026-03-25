#ifndef LOKA_TOOLBOX_RECT_SURFACE_CONTEXT_HPP
#define LOKA_TOOLBOX_RECT_SURFACE_CONTEXT_HPP

#include "app/RectSurface.hpp"
#include <Quickdraw.h>

class ToolboxRectSurfaceContext : public loka::app::scene::NodeContext
{
public:
  explicit ToolboxRectSurfaceContext(loka::app::RectSurfaceNode *node);
  virtual ~ToolboxRectSurfaceContext();

  virtual short layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state);
  virtual void render(loka::app::scene::IPlatformController *);
  void renderDirty(const Rect &dirtyRect);
  bool dirtyRect(Rect &outRect) const;

  void setBoundary(loka::app::scene::BoundaryNode *boundary) { boundary_ = boundary; }

private:
  bool findMatchingCurrentRect(const Rect &previousRect,
                               const loka::app::RectSurfaceModel &model,
                               Rect &currentRect) const;
  void erasePreviousMinusCurrent(const Rect &previousRect, const Rect &currentRect, const Rect &dirtyRect);
  void eraseRectIfVisible(const Rect &rect, const Rect &dirtyRect);
  bool currentModelContainsRect(const Rect &rect, const loka::app::RectSurfaceModel &model) const;
  void noteEraseRect(const Rect &rect);
  void notePaintRect(const Rect &rect);
  void noteFrame(bool usedRegionClip);
  void dumpFrameLog();
  bool buildDirtyRegion(const Rect &dirtyRect, const loka::app::RectSurfaceModel &model);
  void unionSpriteRectsIntoRegion(const loka::app::RectSurfaceModel &model, const Rect &dirtyRect);
  void rememberCurrentModel();

  loka::app::RectSurfaceNode *node_;
  loka::app::scene::BoundaryNode *boundary_;
  Rect rect_;
  loka::app::RectSurfaceModel previousModel_;
  bool hasPreviousModel_;
  RgnHandle dirtyRgn_;
  RgnHandle tempRgn_;
  RgnHandle savedClipRgn_;
  unsigned long loggedFrames_;
  long eraseRectCount_;
  long paintRectCount_;
  unsigned long eraseArea_;
  unsigned long paintArea_;
  long regionClipFrameCount_;
  bool dumpedFrameLog_;
};

#endif
