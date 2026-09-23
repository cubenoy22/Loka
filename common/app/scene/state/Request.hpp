#ifndef LOKA_APP_SCENE_STATE_REQUEST_HPP
#define LOKA_APP_SCENE_STATE_REQUEST_HPP
#include "app/scene/state/Reported.hpp"
#include "app/nodes/controls/TextEditorDocument.hpp"
class MacTextEditorContext;
namespace loka
{
  namespace app
  {
    class TextEditorNode;
    namespace scene
    {
      template <typename T> class RequestSettlement;
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
      /** Internal immutable borrow extracted at the Props door; no public take authority. */
      template <typename T> class RequestBinding
      {
      public:
        RequestBinding()
            : request_(),
              reply_()
        {
        }
        explicit RequestBinding(const WriteSeat<T> &request,
                                const WriteSeat<Reply<T> > &reply = WriteSeat<Reply<T> >())
            : request_(request),
              reply_(reply)
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
          return this->state() == other.state() && this->reply_.state() == other.reply_.state();
        }
        bool operator<(const RequestBinding &other) const
        {
          return this->state() != other.state() ? this->state() < other.state()
                                                : this->reply_.state() < other.reply_.state();
        }

      private:
        friend class RequestSettlement<T>;
        friend class app::TextEditorNode;
        // provisional bridge for #882 b/c/d; removed when each rail moves to settle()
        friend class ::MacTextEditorContext;
        WriteSeat<T> request_;
        WriteSeat<Reply<T> > reply_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka
#endif
