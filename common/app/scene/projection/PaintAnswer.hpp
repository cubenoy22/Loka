#ifndef LOKA_PAINT_ANSWER_HPP
#define LOKA_PAINT_ANSWER_HPP

namespace loka
{
  namespace app
  {
    namespace scene
    {
      enum PaintAnswerKind
      {
        PAINT_ANSWER_EXACT,
        PAINT_ANSWER_NATIVE_SCHEDULED,
        PAINT_ANSWER_REFUSED
      };
      enum PaintRefusalReason
      {
        PAINT_REFUSED_UNSUPPORTED_KIND,
        PAINT_REFUSED_NO_CONTEXT,
        PAINT_REFUSED_PLACEMENT_UNSETTLED,
        PAINT_REFUSED_HISTORY_UNKNOWN,
        PAINT_REFUSED_PROPS_UNRECONCILED
      };
      /** PAINT_ONLY is valid only when the drawer clears its own rectangle;
          otherwise the presenter must erase before painting. */
      enum PaintCoverage
      {
        PAINT_COVERAGE_ERASE_AND_PAINT,
        PAINT_COVERAGE_PAINT_ONLY
      };
      enum PaintPlacementEligibility
      {
        PLACEMENT_ELIGIBLE,
        PLACEMENT_PENDING
      };
      /** Pointer-free presentation coordinates. Keys are rail-defined, never resident addresses. */
      struct PaintScope
      {
        unsigned long ownerKey;
        int originX, originY, clipX, clipY, clipWidth, clipHeight;
        bool operator==(const PaintScope &other) const
        {
          return ownerKey == other.ownerKey && originX == other.originX && originY == other.originY
                 && clipX == other.clipX && clipY == other.clipY && clipWidth == other.clipWidth
                 && clipHeight == other.clipHeight;
        }
        bool operator!=(const PaintScope &other) const
        {
          return !(*this == other);
        }
      };
      /** Pointer-free sufficient damage, not necessarily minimal. */
      struct PaintDamage
      {
        PaintScope scope;
        int x, y, width, height;
        PaintCoverage coverage;
      };
      struct PaintQuery
      {
        PaintScope scope;
        PaintPlacementEligibility placement;
      };
      /** EXACT proves sufficient damage in valid backing; empty means unchanged.
          Native scheduling owes no framework damage; refusal requires widening. */
      struct PaintAnswer
      {
        PaintAnswerKind kind;
        PaintDamage damage;        // Valid only for EXACT.
        PaintRefusalReason reason; // Valid only for REFUSED.
        static PaintAnswer refused(PaintRefusalReason reason)
        {
          PaintAnswer answer = {};
          answer.kind = PAINT_ANSWER_REFUSED;
          answer.reason = reason;
          return answer;
        }
        static PaintAnswer nativeScheduled()
        {
          PaintAnswer answer = {};
          answer.kind = PAINT_ANSWER_NATIVE_SCHEDULED;
          return answer;
        }
        static PaintAnswer exact(const PaintDamage &damage)
        {
          PaintAnswer answer = {};
          answer.kind = PAINT_ANSWER_EXACT;
          answer.damage = damage;
          return answer;
        }
      };
    } // namespace scene
  } // namespace app
} // namespace loka
#endif
