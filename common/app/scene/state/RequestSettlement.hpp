#ifndef LOKA_APP_SCENE_STATE_REQUEST_SETTLEMENT_HPP
#define LOKA_APP_SCENE_STATE_REQUEST_SETTLEMENT_HPP
#include "app/scene/Node.hpp"
#include "app/scene/state/Request.hpp"
namespace loka
{
  namespace app
  {
    namespace scene
    {
      /** The stimulus whose outer operation is completing. */
      enum Settlement
      {
        SETTLE_ATTACH,
        SETTLE_PROPS,
        SETTLE_INPUT,
        SETTLE_RESTORE,
        SETTLE_DEFERRED
      };
      /** Rail decisions; phase changes happen synchronously, arms happen at completion. */
      enum FollowUp
      {
        FOLLOW_NONE,
        RESTORE_QUEUED,
        SCHEDULE_RESTORE,
        SCHEDULE_HIGHLIGHTS,
        OWNER_FOLLOWS,
        SCROLL_CLEANUP,
        REPAINT
      };
      /** Immutable set of returned decisions. Rails resolve scheduling supersession from
          their live phase in finishSettle; independent repaint intent is never lost. */
      class FollowUps
      {
      public:
        FollowUps()
            : bits_(0)
        {
        }
        FollowUps including(FollowUp next) const
        {
          return FollowUps(this->bits_ | (next == FOLLOW_NONE ? 0u : 1u << next));
        }
        bool contains(FollowUp action) const
        {
          return (this->bits_ & (1u << action)) != 0;
        }

      private:
        explicit FollowUps(unsigned bits)
            : bits_(bits)
        {
        }
        unsigned bits_;
      };
      enum Admission
      {
        ADMISSION_EMPTY,
        ADMISSION_DEFERRED,
        ADMISSION_TAKE
      };
      /** A native application result, independent of the seam's acceptance. */
      template <typename T> class RequestApplication
      {
      public:
        RequestApplication(const T &value, EditorResult result, FollowUp follow = FOLLOW_NONE)
            : value_(value),
              result_(result),
              follow_(follow)
        {
        }
        const T &value() const
        {
          return this->value_;
        }
        EditorResult result() const
        {
          return this->result_;
        }
        FollowUp followUp() const
        {
          return this->follow_;
        }

      private:
        T value_;
        EditorResult result_;
        FollowUp follow_;
      };
      /** Stack rail policy. It stores no context borrow; the driver supplies a live Node.
          validate/report are the seam doors and must return the seam's actual result. */
      template <typename T> class RailOperation
      {
      public:
        virtual ~RailOperation() {}
        virtual Admission admit(Node &, RequestBinding<T> &) = 0;
        virtual EditorResult resolve(Node &, const RequestBinding<T> &) = 0;
        virtual EditorResult validate(Node &, const T &) = 0;
        virtual RequestApplication<T> apply(Node &, const T &) = 0;
        virtual EditorResult report(Node &, const T &) = 0;
        virtual bool current(Node &, const RequestBinding<T> &) = 0;
        virtual FollowUp finishTake(Node &, const Reply<T> &) = 0;
        virtual void finishSettle(Node &, const FollowUps &) = 0;
#ifdef TEST_BUILD
        virtual T fact(Node &) const = 0;
#endif
      };
    } // namespace scene
#ifdef TEST_BUILD
    namespace testing
    {
      /** Bounded diagnostic history. Overflow overwrites the oldest row explicitly. */
      template <typename T> struct SettleTraceRow
      {
        scene::Settlement stimulus;
        scene::Admission admission[2];
        scene::Reply<T> takes[2];
        EditorResult seam[2];
        unsigned count;
        T before, after;
        SettleTraceRow(scene::Settlement kind, const T &fact)
            : stimulus(kind),
              count(0),
              before(fact),
              after(fact)
        {
          admission[0] = admission[1] = scene::ADMISSION_EMPTY;
          seam[0] = seam[1] = EDITOR_OK;
        }
      };
      template <typename T> class SettleTrace
      {
      public:
        enum
        {
          CAPACITY = 64
        };
        static SettleTrace &instance()
        {
          static SettleTrace trace;
          return trace;
        }
        void clear()
        {
          this->begin_ = this->size_ = this->overwritten_ = 0;
        }
        unsigned size() const
        {
          return this->size_;
        }
        unsigned overwritten() const
        {
          return this->overwritten_;
        }
        const SettleTraceRow<T> &at(unsigned index) const
        {
          assert(index < this->size_);
          return this->rows_[(this->begin_ + index) % CAPACITY];
        }
        void append(const SettleTraceRow<T> &row)
        {
          if (!row.count && !(row.before != row.after))
            return;
          if (this->size_ == CAPACITY)
          {
            this->begin_ = (this->begin_ + 1) % CAPACITY;
            --this->size_;
            ++this->overwritten_;
          }
          static_cast<SettleTraceRow<T> &>(this->rows_[(this->begin_ + this->size_++) % CAPACITY]) = row;
        }

      private:
        // Rows are values; no trace retains an owner, node, context or state handle.
        struct Row : SettleTraceRow<T>
        {
          Row()
              : SettleTraceRow<T>(scene::SETTLE_ATTACH, T())
          {
          }
        };
        SettleTrace()
            : begin_(0),
              size_(0),
              overwritten_(0)
        {
        }
        Row rows_[CAPACITY];
        unsigned begin_, size_, overwritten_;
      };
    } // namespace testing
#endif
    namespace scene
    {
      /** Owns the bounded take protocol. Node reclamation is deferred by its owner clock;
          context retirement is synchronous. identity is compared, never dereferenced. */
      template <typename T> class RequestSettlement
      {
      public:
        static void settle(Node *node,
                           const NodeContext *identity,
                           RailOperation<T> &op,
                           Settlement stimulus
#ifdef TEST_BUILD
                           ,
                           const T &before
#endif
        )
        {
          if (!alive(node, identity))
            return;
          FollowUps follow;
#ifdef TEST_BUILD
          loka::app::testing::SettleTraceRow<T> row(stimulus, before);
#else
          (void)stimulus;
#endif
          if (!take(node,
                    identity,
                    op,
                    follow
#ifdef TEST_BUILD
                    ,
                    row,
                    0
#endif
                    ))
            return;
          if (!take(node,
                    identity,
                    op,
                    follow
#ifdef TEST_BUILD
                    ,
                    row,
                    1
#endif
                    ))
            return;
          op.finishSettle(*node, follow);
          if (!alive(node, identity))
            return;
#ifdef TEST_BUILD
          row.after = op.fact(*node);
          loka::app::testing::SettleTrace<T>::instance().append(row);
#endif
        }

      private:
        static bool alive(Node *node, const NodeContext *identity)
        {
          return node && node->getContext() == identity;
        }
        static bool take(Node *node,
                         const NodeContext *identity,
                         RailOperation<T> &op,
                         FollowUps &follow
#ifdef TEST_BUILD
                         ,
                         loka::app::testing::SettleTraceRow<T> &row,
                         unsigned ordinal
#endif
        )
        {
          RequestBinding<T> binding;
          const Admission admission = op.admit(*node, binding);
#ifdef TEST_BUILD
          row.admission[ordinal] = admission;
#endif
          if (admission != ADMISSION_TAKE)
            return true;
          const T pending = binding.state()->get();
          binding.request_.set(T::None());
          if (!alive(node, identity))
            return false;
          EditorResult result = op.resolve(*node, binding);
          if (result == EDITOR_OK)
            result = op.validate(*node, pending);
          RequestApplication<T> applied(pending, result);
          if (result == EDITOR_OK)
            applied = op.apply(*node, pending);
          if (!alive(node, identity))
            return false;
          follow = follow.including(applied.followUp());
          result = applied.result();
          if (result == EDITOR_OK)
            result = op.report(*node, applied.value());
          if (!alive(node, identity))
            return false;
          const Reply<T> reply = result != EDITOR_OK          ? Reply<T>::Refused(pending, result)
                                 : pending != applied.value() ? Reply<T>::Clamped(pending, applied.value())
                                                              : Reply<T>::Granted(applied.value());
          // A binding discard is not a reply to a different recipient.
          if (op.current(*node, binding))
            binding.reply_.set(reply, true);
          if (!alive(node, identity))
            return false;
#ifdef TEST_BUILD
          row.takes[row.count] = reply;
          row.seam[row.count++] = result;
#endif
          const FollowUp completed = op.finishTake(*node, reply);
          if (!alive(node, identity))
            return false;
          follow = follow.including(completed);
          return true;
        }
      };
    } // namespace scene
  } // namespace app
} // namespace loka
#endif
