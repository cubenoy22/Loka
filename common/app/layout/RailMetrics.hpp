#ifndef LOKA_APP_LAYOUT_RAIL_METRICS_HPP
#define LOKA_APP_LAYOUT_RAIL_METRICS_HPP

#include <cassert>

namespace loka
{
  namespace app
  {
    /** Positive integer ratio. Equality compares the stored pair, without
        normalization. Invalid construction is a contract violation; valid()
        also permits validation when assertions are disabled. */
    struct Ratio
    {
      int num;
      int den;

      Ratio(int numerator = 1, int denominator = 1)
          : num(numerator), den(denominator)
      {
        assert(this->valid());
      }

      bool valid() const { return this->num > 0 && this->den > 0; }
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
          : fontScale(font), spaceScale(space)
      {
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
