#ifndef LOKA_TOOLBOX_RECT_SURFACE_CONTEXT_HPP
#define LOKA_TOOLBOX_RECT_SURFACE_CONTEXT_HPP

#include "context/ToolboxProjectedNodeContext.hpp"
#include "app/RectSurface.hpp"
#include <Quickdraw.h>

class ToolboxRectSurfaceContext : public ToolboxProjectedNodeContext
{
public:
  ToolboxRectSurfaceContext(loka::app::RectSurfaceNode *node, ToolboxScenePlatformController *controller);
  virtual ~ToolboxRectSurfaceContext();
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                             loka::app::scene::NodeLifecycleFact next);

  virtual short layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state);
  virtual void render(loka::app::scene::IPlatformController *);
  void renderDirty(const Rect &dirtyRect);
  bool dirtyRect(Rect &outRect) const;

private:
  bool
  findMatchingCurrentRect(const Rect &previousRect, const loka::app::RectSurfaceModel &model, Rect &currentRect) const;
  Rect rectForSprite(const loka::app::RectSurfaceSprite &sprite) const;
  bool previousRectForIndex(short index, Rect &previousRect) const;
  void paintCurrentMinusPrevious(const Rect &currentRect, const Rect &previousRect, const Rect &dirtyRect);
  void paintRectIfVisible(const Rect &rect, const Rect &dirtyRect);
  loka::app::RectSurfacePaintResult
  paintImage(const loka::app::RectSurfaceSprite &sprite, const Rect &rect, const Rect *dirtyRect);
  void erasePreviousMinusCurrent(const Rect &previousRect, const Rect &currentRect, const Rect &dirtyRect);
  void eraseRectIfVisible(const Rect &rect, const Rect &dirtyRect);
  bool currentModelContainsRect(const Rect &rect, const loka::app::RectSurfaceModel &model) const;
  bool modelContainsImage(const loka::app::RectSurfaceModel &model) const;
  bool buildDirtyRegion(const Rect &dirtyRect, const loka::app::RectSurfaceModel &model);
  void unionSpriteRectsIntoRegion(const loka::app::RectSurfaceModel &model, const Rect &dirtyRect);
  void finishPaint(loka::app::RectSurfacePaintResult result,
                   const loka::app::RectSurfaceModel &requestedModel);

  loka::app::RectSurfaceNode *node_;
  Rect rect_;
  loka::app::RectSurfaceModel previousModel_;
  bool hasPreviousModel_;
  RgnHandle dirtyRgn_;
  RgnHandle tempRgn_;
  RgnHandle savedClipRgn_;
};

void EnsureToolboxRectSurfaceContext(loka::app::RectSurfaceNode *node, ToolboxScenePlatformController *controller);

#endif
