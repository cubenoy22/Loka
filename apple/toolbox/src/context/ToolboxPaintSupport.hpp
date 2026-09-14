#ifndef LOKA_TOOLBOX_PAINT_SUPPORT_HPP
#define LOKA_TOOLBOX_PAINT_SUPPORT_HPP

#include <Quickdraw.h>
#include "app/scene/projection/PaintFact.hpp"
#include "app/scene/Node.hpp"

/** Visit-local convention: every Toolbox answer names pixels in its owning
    window. The rail consumes it before returning from onBoundaryApply. */
inline loka::app::scene::PaintScope ToolboxPaintScope()
{
  const loka::app::scene::PaintScope scope = {1, 0, 0, 0, 0, 0, 0};
  return scope;
}

/** A settled, attached layout clipped entirely out of the viewport owes no
    visible pixels, even without paint history. A default/unplaced rectangle
    or detached context is not evidence of clipping. Call after query scope
    and props checks; layout changes must still use the broad delivery path. */
inline bool ToolboxPaintIsClippedOut(const Rect &layoutRect, const Rect &paintRect,
                                    loka::app::scene::NodeLifecycleFact fact)
{
  return fact == loka::app::scene::NODE_FACT_ATTACHED
         && !EmptyRect(&layoutRect) && EmptyRect(&paintRect);
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

/** Stack-owned QuickDraw clip. REPLACE preserves legacy broad Text drawing;
    exact delivery uses INTERSECT. Nested drawing
    cannot consume another caller's save, including on allocation refusal. */
class ToolboxPaintClip
{
public:
  enum Operation { INTERSECT, REPLACE };
  explicit ToolboxPaintClip(const Rect &rect, Operation operation = INTERSECT)
      : saved_(NewRgn()), clip_(NewRgn())
  {
    if (!this->isActive())
      return;
    GetClip(this->saved_);
    RectRgn(this->clip_, &rect);
    if (operation == INTERSECT)
      SectRgn(this->saved_, this->clip_, this->clip_);
    SetClip(this->clip_);
  }
  ~ToolboxPaintClip()
  {
    if (this->isActive())
      SetClip(this->saved_);
    if (this->clip_)
      DisposeRgn(this->clip_);
    if (this->saved_)
      DisposeRgn(this->saved_);
  }
  bool isActive() const { return this->saved_ && this->clip_; }
  /** Unknown coverage must still draw under the caller's clip on refusal. */
  bool touches(const Rect &rect) const
  {
    return !this->isActive() || RectInRgn(&rect, this->clip_);
  }
  bool covers(const Rect &rect) const
  {
    return this->isActive() && ToolboxPaintClipCovers(this->clip_, rect);
  }
private:
  ToolboxPaintClip(const ToolboxPaintClip &);
  ToolboxPaintClip &operator=(const ToolboxPaintClip &);
  RgnHandle saved_;
  RgnHandle clip_;
};

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
