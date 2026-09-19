#ifndef LOKA_APP_LAYOUT_RAIL_METRICS_HPP
#define LOKA_APP_LAYOUT_RAIL_METRICS_HPP

#include <cassert>

namespace loka
{
  namespace app
  {
    /** Integer ratio of at least one: rails only scale logical units up
        (#818), so num >= den > 0 is the whole contract. Equality compares the
        stored pair without normalization. Construction does not assert so a
        refused value can be inspected; RailMetrics refuses invalid ratios by
        falling back to the unit ratio in every build (assert in debug). */
    struct Ratio
    {
      int num;
      int den;

      Ratio(int numerator = 1, int denominator = 1)
          : num(numerator), den(denominator)
      {
      }

      bool valid() const { return this->den > 0 && this->num >= this->den; }
      bool isUnit() const { return this->valid() && this->num == this->den; }
      bool operator==(const Ratio &rhs) const
      {
        return this->num == rhs.num && this->den == rhs.den;
      }
      bool operator!=(const Ratio &rhs) const { return !(*this == rhs); }
    };

    /** Rail-owned layout facts, separate from actual display DPI. spaceScale
        applies only at the rail's projection of layout results; layout stays
        in logical units (lu). fontScale applies to the Text font-size table.
        The default value is 1/1 for both ratios. */
    struct RailMetrics
    {
      Ratio fontScale;
      Ratio spaceScale;

      RailMetrics(const Ratio &font = Ratio(), const Ratio &space = Ratio())
          : fontScale(font.valid() ? font : Ratio()), spaceScale(space.valid() ? space : Ratio())
      {
        assert(font.valid() && space.valid() && "RailMetrics ratios must be >= 1; refused to the unit ratio");
      }

      bool operator==(const RailMetrics &rhs) const
      {
        return this->fontScale == rhs.fontScale && this->spaceScale == rhs.spaceScale;
      }
      bool operator!=(const RailMetrics &rhs) const { return !(*this == rhs); }
    };
  } // namespace app
} // namespace loka

#endif // LOKA_APP_LAYOUT_RAIL_METRICS_HPP
