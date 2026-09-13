#ifndef LOKA_TOOLBOX_PAINT_SUPPORT_HPP
#define LOKA_TOOLBOX_PAINT_SUPPORT_HPP

#include <Quickdraw.h>
#include "app/scene/projection/PaintFact.hpp"

/** Visit-local convention: every Toolbox answer names pixels in its owning
    window. The rail consumes it before returning from onBoundaryApply. */
inline loka::app::scene::PaintScope ToolboxPaintScope()
{
  const loka::app::scene::PaintScope scope = {1, 0, 0, 0, 0, 0, 0};
  return scope;
}

/** Only a rectangular clip covering the entire projected visible placement
    can establish a completed paint fact. Complex/partial clips refuse history. */
inline bool ToolboxPaintClipCovers(RgnHandle clip, const Rect &rect)
{
  if (!clip || !*clip || (**clip).rgnSize != sizeof(Region))
    return false;
  const Rect &bounds = (**clip).rgnBBox;
  return bounds.left <= rect.left && bounds.top <= rect.top
         && bounds.right >= rect.right && bounds.bottom >= rect.bottom;
}

inline loka::app::scene::PaintAnswer ToolboxExactPaint(const Rect &rect, bool changed)
{
  using namespace loka::app::scene;
  const PaintDamage damage = {ToolboxPaintScope(), rect.left, rect.top,
                              changed ? rect.right - rect.left : 0,
                              changed ? rect.bottom - rect.top : 0,
                              PAINT_COVERAGE_PAINT_ONLY};
  return PaintAnswer::exact(damage);
}

#endif
