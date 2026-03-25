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
  loka::app::RectSurfaceNode *node_;
  loka::app::scene::BoundaryNode *boundary_;
  Rect rect_;
};

#endif
