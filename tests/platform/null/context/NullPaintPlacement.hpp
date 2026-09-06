#ifndef LOKA_NULL_PAINT_PLACEMENT_HPP
#define LOKA_NULL_PAINT_PLACEMENT_HPP
#include "app/scene/projection/PaintFact.hpp"
#include "core/Frame.hpp"
/** Optional completed seat in the single root presentation space. */
class NullPaintPlacement
{
public:
  void complete(const loka::core::Frame &seat, const loka::app::scene::PaintScope &scope)
  {
    this->seat_.commit(seat, scope);
  }
  void invalidate()
  {
    this->seat_.invalidate();
  }
  bool query(const loka::app::scene::PaintScope &scope, loka::core::Frame &seat) const
  {
    if (!this->seat_.isKnown() || this->seat_.scope() != scope)
      return false;
    seat = this->seat_.value();
    return true;
  }

private:
  loka::app::scene::PaintFact<loka::core::Frame> seat_;
};
#endif
