#ifndef LOKA_COLLECT_PAINT_ANSWERS_HPP
#define LOKA_COLLECT_PAINT_ANSWERS_HPP

#include "app/scene/projection/ApplyPaintPlan.hpp"
#include "app/scene/projection/PaintEnumeration.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      /** Borrowed resident plus its sufficient, non-empty presentation damage.
          Consume inside the rail's onBoundaryApply; never queue the resident. */
      struct PaintAnswerRecord
      {
        Node *resident;
        PaintDamage damage;
      };

      /** Caller-owned stack storage. Collection clears it before each visit;
          leaving onBoundaryApply ends all resident borrows. No heap fallback. */
      template <unsigned Capacity = kApplyPaintPlanCapacity> class PaintAnswerBuffer
      {
      public:
        PaintAnswerBuffer()
            : count_(0)
        {
        }
        void clear()
        {
          this->count_ = 0;
        }
        bool append(Node *resident, const PaintDamage &damage)
        {
          if (this->count_ == Capacity)
            return false;
          this->entries_[this->count_].resident = resident;
          this->entries_[this->count_].damage = damage;
          ++this->count_;
          return true;
        }
        unsigned count() const
        {
          return this->count_;
        }
        const PaintAnswerRecord &entry(unsigned index) const
        {
          assert(index < this->count_);
          return this->entries_[index];
        }

      private:
        PaintAnswerBuffer(const PaintAnswerBuffer &);
        PaintAnswerBuffer &operator=(const PaintAnswerBuffer &);
        typedef char SupportedCapacity[Capacity > 0 && Capacity <= kApplyPaintPlanCapacity ? 1 : -1];
        unsigned count_;
        PaintAnswerRecord entries_[Capacity];
      };

      /** Completed visit facts. EXACT includes empty answers; overflow counts
          non-empty EXACT answers refused by the caller's buffer. A scope mismatch
          is normalized to a placement refusal before it enters these counts. */
      class PaintApplyVerdict
      {
      public:
        PaintApplyVerdict(unsigned exact,
                          unsigned nativeScheduled,
                          unsigned refused,
                          unsigned overflow,
                          ApplyPaintWidenReason reason,
                          PaintRefusalReason refusal)
            : exact_(exact),
              nativeScheduled_(nativeScheduled),
              refused_(refused),
              overflow_(overflow),
              reason_(reason),
              refusal_(refusal)
        {
        }
        unsigned exactCount() const
        {
          return this->exact_;
        }
        unsigned nativeScheduledCount() const
        {
          return this->nativeScheduled_;
        }
        unsigned refusedCount() const
        {
          return this->refused_;
        }
        unsigned overflowCount() const
        {
          return this->overflow_;
        }
        bool widened() const
        {
          return this->refused_ != 0 || this->overflow_ != 0;
        }
        ApplyPaintWidenReason widenReason() const
        {
          return this->reason_;
        }
        PaintRefusalReason refusalReason() const
        {
          return this->refusal_;
        }
        bool canSkipBroadPaint(const BoundaryLocalApplyInfo &info) const
        {
          // NATIVE_SCHEDULED is handled: the context already scheduled its own
          // native delivery. Context-owned invalidation remains in place; only
          // the rail's broad request is eligible for suppression. Empty EXACT
          // answers are also handled (unchanged). Composited work still needs
          // the existing replay path, even when every drawer was handled.
          return info.hasPaintWork() && !info.hasStructureWork && !info.hasLayoutWork && !info.hasCompositedPaintWork()
                 && !this->widened();
        }

      private:
        const unsigned exact_, nativeScheduled_, refused_, overflow_;
        const ApplyPaintWidenReason reason_;
        const PaintRefusalReason refusal_;
      };

      namespace paint_detail
      {
        /** Mutable construction phase; no facts or borrowed rows escape until
            enumeration returns the completed verdict to the caller. */
        template <unsigned Capacity, class Source> class AnswerCollector : public IPaintResidentVisitor
        {
        public:
          AnswerCollector(const PaintQuery &query, PaintAnswerBuffer<Capacity> &answers, Source &source)
              : query_(query),
                answers_(answers),
                source_(source),
                exact_(0),
                nativeScheduled_(0),
                refused_(0),
                overflow_(0),
                reason_(APPLY_PAINT_WIDEN_UNSUPPORTED),
                refusal_(PAINT_REFUSED_UNSUPPORTED_KIND)
          {
          }
          virtual void visit(Node *resident, NodeContext *context, BoundaryNode *)
          {
            PaintAnswer answer;
            if (!this->source_.queryPaintAnswer(resident, context, this->query_, answer))
              return;
            if (answer.kind == PAINT_ANSWER_EXACT && answer.damage.scope != this->query_.scope)
              answer = PaintAnswer::refused(PAINT_REFUSED_PLACEMENT_UNSETTLED);
            switch (answer.kind)
            {
            case PAINT_ANSWER_EXACT:
              ++this->exact_;
              if (answer.damage.width > 0 && answer.damage.height > 0
                  && !this->answers_.append(resident, answer.damage))
              {
                this->noteWiden(APPLY_PAINT_WIDEN_CAPACITY, PAINT_REFUSED_UNSUPPORTED_KIND);
                ++this->overflow_;
              }
              break;
            case PAINT_ANSWER_NATIVE_SCHEDULED:
              ++this->nativeScheduled_;
              break;
            case PAINT_ANSWER_REFUSED:
              this->noteWiden(APPLY_PAINT_WIDEN_REFUSED, answer.reason);
              ++this->refused_;
              break;
            }
          }
          PaintApplyVerdict verdict() const
          {
            return PaintApplyVerdict(
                this->exact_, this->nativeScheduled_, this->refused_, this->overflow_, this->reason_, this->refusal_);
          }

        private:
          void noteWiden(ApplyPaintWidenReason reason, PaintRefusalReason refusal)
          {
            if (this->refused_ == 0 && this->overflow_ == 0)
            {
              this->reason_ = reason;
              this->refusal_ = refusal;
            }
          }
          const PaintQuery &query_;
          PaintAnswerBuffer<Capacity> &answers_;
          Source &source_;
          unsigned exact_, nativeScheduled_, refused_, overflow_;
          ApplyPaintWidenReason reason_;
          PaintRefusalReason refusal_;
        };
      } // namespace paint_detail

      /** One existing attached-descendant traversal, including nested Boundaries.
          The rail's Source::queryPaintAnswer returns false for non-drawers and
          otherwise fills one answer using its context installation contract.
          Source must query synchronously without changing the tree, contexts, or
          presentation history. Common never casts a context or invents a native
          scope/delivery policy. Consume records before detach/context replacement;
          BuildApplyPaintPlan must receive the same query scope. */
      template <unsigned Capacity, class Source>
      PaintApplyVerdict CollectPaintAnswers(BoundaryNode &root,
                                            const PaintQuery &query,
                                            PaintAnswerBuffer<Capacity> &answers,
                                            Source &source)
      {
        answers.clear();
        paint_detail::AnswerCollector<Capacity, Source> collector(query, answers, source);
        enumerateAttachedResidents(&root, collector);
        return collector.verdict();
      }

      /** Adapt completed answers to the legacy pointer-free presentation value.
          Walks only the caller's bounded buffer, never residents or contexts. */
      template <unsigned Capacity>
      ApplyPaintPlan BuildApplyPaintPlan(const PaintScope &scope,
                                         const PaintAnswerBuffer<Capacity> &answers,
                                         const PaintApplyVerdict &verdict)
      {
        ApplyPaintPlan plan;
        if (verdict.widened())
          plan.widen(verdict.widenReason(), scope, verdict.refusalReason());
        else
          for (unsigned i = 0; i < answers.count(); ++i)
            // PaintAnswerBuffer statically bounds Capacity by the plan's limit.
            plan.addExact(answers.entry(i).damage);
        return plan;
      }
    } // namespace scene
  } // namespace app
} // namespace loka
#endif
