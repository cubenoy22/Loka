#ifndef LOKA_APP_SCENE_STATE_REQUEST_HPP
#define LOKA_APP_SCENE_STATE_REQUEST_HPP
#include "app/scene/state/Reported.hpp"
#include "app/nodes/controls/TextEditorDocument.hpp"
namespace loka
{
  namespace app
  {
    class TextEditorNode;
    namespace scene
    {
      template <typename T> class RequestSettlement;
      template <typename T> class RequestBinding;
      template <typename T> struct RequestTraits;
      template <> struct RequestTraits<LineCursor>
      {
        enum { coalescable = 1 };
      };
      /** Plain slots are only meaningful for explicitly coalescable values. */
      template <typename T> struct RequestDeclarationWall
      {
#if __cplusplus >= 201103L
        static_assert(RequestTraits<T>::coalescable, "Use RequestQueue for non-coalescable requests");
#endif
        typedef char Wall[RequestTraits<T>::coalescable ? 1 : -1];
      };
      /** Admission to an endpoint; refused posts change neither slot nor ring. */
      enum PostResult
      {
        POST_ACCEPTED,
        POST_QUEUE_FULL,
        POST_INVALID
      };
      /** Result of one take, not a promise for every post. */
      template <typename T> class Reply
      {
      public:
        enum Kind
        {
          NO_REPLY,
          GRANTED,
          CLAMPED,
          REFUSED
        };
        Reply()
            : kind_(NO_REPLY),
              requested_(),
              applied_(),
              reason_(EDITOR_OK)
        {
        }
        static Reply Granted(const T &applied)
        {
          return Reply(GRANTED, applied, applied, EDITOR_OK);
        }
        static Reply Clamped(const T &requested, const T &applied)
        {
          return Reply(CLAMPED, requested, applied, EDITOR_OK);
        }
        static Reply Refused(const T &requested, EditorResult reason)
        {
          return Reply(REFUSED, requested, T(), reason);
        }
        Kind kind() const
        {
          return this->kind_;
        }
        const T &requested() const
        {
          assert(this->kind_ != NO_REPLY);
          return this->requested_;
        }
        const T &applied() const
        {
          assert(this->kind_ == GRANTED || this->kind_ == CLAMPED);
          return this->applied_;
        }
        EditorResult reason() const
        {
          assert(this->kind_ == REFUSED);
          return this->reason_;
        }
        bool operator!=(const Reply &other) const
        {
          return this->kind_ != other.kind_ || this->requested_ != other.requested_ || this->applied_ != other.applied_
                 || this->reason_ != other.reason_;
        }

      private:
        Reply(Kind kind, const T &requested, const T &applied, EditorResult reason)
            : kind_(kind),
              requested_(requested),
              applied_(applied),
              reason_(reason)
        {
        }
        Kind kind_;
        T requested_;
        T applied_;
        EditorResult reason_;
      };
      template <typename T> class RequestWithReply;
      /** App-owned declaration handle. Borrow by reference; None means no request. */
      template <typename T> class Request
      {
      public:
        Request()
            : seat_()
        {
        }
        bool isValid() const
        {
          return this->seat_.isValid();
        }
        core::State<T> *state() const
        {
          return this->seat_.state();
        }
        T get() const
        {
          return this->seat_.get();
        }
        void set(const T &value)
        {
          this->seat_.set(value);
        }

      private:
        friend class ComposableNode;
        friend class StateBatchBase;
        template <class PropsT> friend struct NodePropsBase;
        Request(const Request &);
        Request &operator=(const Request &);
        NodeState<T> seat_;
      };
      /** Opt-in per-take replies. The reference conversion is a posting view;
          pass this typed handle to Props to include the reply capability.
          Declaration handles cannot be copied or sliced into a plain request. */
      template <typename T> class RequestWithReply
      {
      public:
        RequestWithReply()
            : request_(),
              reply_()
        {
        }
        operator Request<T> &()
        {
          return this->request_;
        }
        bool isValid() const
        {
          return this->request_.isValid();
        }
        core::State<T> *state() const
        {
          return this->request_.state();
        }
        T get() const
        {
          return this->request_.get();
        }
        void set(const T &value)
        {
          this->request_.set(value);
        }
        const Reported<Reply<T> > &reply() const
        {
          return this->reply_;
        }

      private:
        friend class ComposableNode;
        friend class StateBatchBase;
        template <class PropsT> friend struct NodePropsBase;
        RequestWithReply(const RequestWithReply &);
        RequestWithReply &operator=(const RequestWithReply &);
        Request<T> request_;
        Reported<Reply<T> > reply_;
      };
      /** App-owned FIFO endpoint. Borrow by reference without extending its lifetime.
          Accepted posts receive one reply unless binding change or detach discards
          them. Cancellation gives neither per-item replies nor an exact drop count.
          Storage is supplied by RequestQueue; no queue operation allocates. */
      template <typename T> class RequestQueueBase
      {
      public:
        PostResult post(const T &value)
        {
          if (value.isNone())
            return POST_INVALID;
          if (this->seat_.get().isNone() && this->count_ == 0)
          {
            this->seat_.set(value);
            return POST_ACCEPTED;
          }
          if (this->count_ == this->cap_)
            return POST_QUEUE_FULL;
          this->ring_[(this->head_ + this->count_) % this->cap_] = value;
          ++this->count_;
          return POST_ACCEPTED;
        }
        /** Current ring occupancy, excluding the published slot. */
        unsigned pending() const { return this->count_; }
        const Reported<Reply<T> > &reply() const { return this->reply_; }
        bool isValid() const { return this->seat_.isValid(); }
        core::State<T> *state() const { return this->seat_.state(); }

      protected:
        RequestQueueBase(T *ring, unsigned char capacity)
            : seat_(), reply_(), ring_(ring), cap_(capacity), head_(0), count_(0) {}

      private:
        friend class RequestBinding<T>;
        friend class ComposableNode;
        friend class StateBatchBase;
        template <class PropsT> friend struct NodePropsBase;
        RequestQueueBase(const RequestQueueBase &);
        RequestQueueBase &operator=(const RequestQueueBase &);
        bool advance(T &next)
        {
          if (!this->count_)
            return false;
          next = this->ring_[this->head_];
          // Vacated entries hold None so a payload that owns a resource is not
          // retained until the position is overwritten.
          this->ring_[this->head_] = T::None();
          this->head_ = (this->head_ + 1) % this->cap_;
          --this->count_;
          return true;
        }
        void clearRing()
        {
          while (this->count_)
          {
            this->ring_[this->head_] = T::None();
            this->head_ = (this->head_ + 1) % this->cap_;
            --this->count_;
          }
          this->head_ = 0;
        }
        NodeState<T> seat_;
        Reported<Reply<T> > reply_;
        T *ring_;
        unsigned char cap_, head_, count_;
      };
      /** Fixed ring of 1..255 waiting values, plus the endpoint's published slot. */
      template <typename T, unsigned N> class RequestQueue : public RequestQueueBase<T>
      {
        typedef char CapacityWall[(N >= 1 && N <= 255) ? 1 : -1];
      public:
        RequestQueue() : RequestQueueBase<T>(this->storage_, N)
        {
          (void)sizeof(CapacityWall);
        }
      private:
        T storage_[N];
      };
      /** Internal borrow extracted at the Props door; consume/discard are friend-only doors. */
      template <typename T> class RequestBinding
      {
      public:
        RequestBinding()
            : request_(),
              reply_(),
              source_(0)
        {
        }
        explicit RequestBinding(const WriteSeat<T> &request,
                                const WriteSeat<Reply<T> > &reply = WriteSeat<Reply<T> >(),
                                RequestQueueBase<T> *source = 0)
            : request_(request),
              reply_(reply),
              source_(source)
        {
        }
        bool isValid() const
        {
          return this->request_.isValid();
        }
        core::State<T> *state() const
        {
          return this->request_.state();
        }
        bool usesTracker(const core::StateTracker *tracker) const
        {
          return this->request_.usesTracker(tracker) && (!this->reply_.isValid() || this->reply_.usesTracker(tracker));
        }
        bool same(const RequestBinding &other) const
        {
          return this->state() == other.state() && this->reply_.state() == other.reply_.state()
                 && this->source_ == other.source_;
        }
        bool operator<(const RequestBinding &other) const
        {
          if (this->state() != other.state())
            return this->state() < other.state();
          if (this->reply_.state() != other.reply_.state())
            return this->reply_.state() < other.reply_.state();
          return this->source_ < other.source_;
        }

      private:
        friend class RequestSettlement<T>;
        friend class app::TextEditorNode;
        T consume() const
        {
          const T taken = this->request_.state()->get();
          T next;
          if (this->source_ && this->source_->advance(next))
            this->request_.set(next, true);
          else
            this->request_.set(T::None());
          return taken;
        }
        void discard() const
        {
          if (this->source_)
            this->source_->clearRing();
          if (this->request_.isValid() && !this->request_.state()->get().isNone())
            this->request_.set(T::None());
        }
        WriteSeat<T> request_;
        WriteSeat<Reply<T> > reply_;
        RequestQueueBase<T> *source_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka
#endif
