#ifndef LOKA_APPLY_PAINT_PLAN_HPP
#define LOKA_APPLY_PAINT_PLAN_HPP
#include <cassert>
#include "app/scene/projection/PaintAnswer.hpp"
namespace loka
{
  namespace app
  {
    namespace scene
    {
      enum ApplyPaintPrecision
      {
        APPLY_PAINT_NONE,
        APPLY_PAINT_EXACT,
        APPLY_PAINT_WIDENED
      };
      enum ApplyPaintWidenReason
      {
        APPLY_PAINT_WIDEN_REFUSED,
        APPLY_PAINT_WIDEN_CAPACITY,
        APPLY_PAINT_WIDEN_UNSUPPORTED
      };
      enum
      {
        kApplyPaintPlanCapacity = 8
      };
      /** Pointer-free visit-local value. The platform consumes this independently of
          the Boundary hook's legacy BoundaryLocalApplyInfo; they do not share a plan. */
      class ApplyPaintPlan
      {
      public:
        ApplyPaintPlan()
            : precision_(APPLY_PAINT_NONE),
              reason_(APPLY_PAINT_WIDEN_UNSUPPORTED),
              refusal_(PAINT_REFUSED_UNSUPPORTED_KIND),
              count_(0),
              entries_()
        {
        }
        bool addExact(const PaintDamage &damage)
        {
          if (this->precision_ == APPLY_PAINT_WIDENED)
            return false;
          if (damage.width <= 0 || damage.height <= 0)
            return true;
          if (this->count_ == kApplyPaintPlanCapacity)
            return false;
          this->entries_[this->count_++] = damage;
          this->precision_ = APPLY_PAINT_EXACT;
          return true;
        }
        void widen(ApplyPaintWidenReason reason,
                   const PaintScope &scope,
                   PaintRefusalReason refusal = PAINT_REFUSED_UNSUPPORTED_KIND)
        {
          if (this->precision_ == APPLY_PAINT_WIDENED)
            return;
          this->precision_ = APPLY_PAINT_WIDENED;
          this->reason_ = reason;
          this->refusal_ = refusal;
          this->count_ = 1;
          const PaintDamage damage = {
              scope, scope.clipX, scope.clipY, scope.clipWidth, scope.clipHeight, PAINT_COVERAGE_ERASE_AND_PAINT};
          this->entries_[0] = damage;
        }
        ApplyPaintPrecision precision() const
        {
          return this->precision_;
        }
        ApplyPaintWidenReason widenReason() const
        {
          return this->reason_;
        }
        PaintRefusalReason refusalReason() const
        {
          return this->refusal_;
        }
        unsigned count() const
        {
          return this->count_;
        }
        unsigned exactCount() const
        {
          return this->precision_ == APPLY_PAINT_EXACT ? this->count_ : 0;
        }
        const PaintDamage &entry(unsigned i) const
        {
          assert(i < this->count_);
          return this->entries_[i];
        }

      private:
        ApplyPaintPrecision precision_;
        ApplyPaintWidenReason reason_;
        PaintRefusalReason refusal_;
        unsigned count_;
        PaintDamage entries_[kApplyPaintPlanCapacity];
      };
    } // namespace scene
  } // namespace app
} // namespace loka
#endif
