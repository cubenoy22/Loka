#ifndef LOKA_APP_MEASUREMENT_RESULT_HPP
#define LOKA_APP_MEASUREMENT_RESULT_HPP

namespace loka
{
  namespace app
  {
    /** Completed intrinsic geometry under a rail-typed measurement constraint.
        Only a successful build may commit. Refusal invalidates reuse; placement
        remains the caller's work on every layout, including a measurement hit.
        Initial/fallback geometry is readable but never reusable. */
    template <class Constraint, class Extent> class MeasurementResult
    {
    public:
      explicit MeasurementResult(const Extent &fallback = Extent())
          : constraint_(),
            extent_(fallback),
            completed_(false)
      {
      }
      void commit(const Constraint &constraint, const Extent &extent)
      {
        this->constraint_ = constraint;
        this->extent_ = extent;
        this->completed_ = true;
      }
      void invalidate()
      {
        this->completed_ = false;
      }
      bool reusable(const Constraint &constraint) const
      {
        return this->completed_ && this->constraint_ == constraint;
      }
      const Extent &extent() const
      {
        return this->extent_;
      }

    private:
      Constraint constraint_;
      Extent extent_;
      bool completed_;
    };
  } // namespace app
} // namespace loka
#endif
