#ifndef LOKA_TOOLBOX_RECT_SURFACE_CONTEXT_HPP
#define LOKA_TOOLBOX_RECT_SURFACE_CONTEXT_HPP

#include "context/ToolboxProjectedNodeContext.hpp"
#include "app/RectSurface.hpp"
#include "context/ToolboxPaintSupport.hpp"
#include <Quickdraw.h>

class ToolboxRectSurfaceContext : public ToolboxProjectedNodeContext
{
public:
  ToolboxRectSurfaceContext(loka::app::RectSurfaceNode *node, ToolboxScenePlatformController *controller);
  virtual ~ToolboxRectSurfaceContext();
  virtual loka::app::scene::PaintAnswer queryPaintDamage(const loka::app::scene::PaintQuery &query) const;
  virtual void onPropsApplied();
  virtual void onFactChanged(loka::app::scene::NodeLifecycleFact previous,
                             loka::app::scene::NodeLifecycleFact next);

  virtual short layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state);
  virtual void render(loka::app::scene::IPlatformController *);
  void renderDirty(const Rect &dirtyRect);
  bool dirtyRect(Rect &outRect) const;

private:
  Rect rectForSprite(const loka::app::RectSprite &sprite) const;
  bool buildDirtyRegion(const Rect &dirtyRect, const loka::app::RectSurfaceModel &model);
  void unionSpriteRectsIntoRegion(const loka::app::RectSurfaceModel &model, const Rect &dirtyRect);

  loka::app::RectSurfaceNode *node_;
  Rect rect_;
  Rect paintRect_;
  loka::app::scene::PaintFact<loka::app::RectSurfaceModel> presented_;
  RgnHandle dirtyRgn_;
  RgnHandle tempRgn_;
  RgnHandle savedClipRgn_;
};

void EnsureToolboxRectSurfaceContext(loka::app::RectSurfaceNode *node, ToolboxScenePlatformController *controller);

#endif
