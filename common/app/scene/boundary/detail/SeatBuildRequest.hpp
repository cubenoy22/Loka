#ifndef LOKA_SEAT_BUILD_REQUEST_HPP
#define LOKA_SEAT_BUILD_REQUEST_HPP

#include "app/scene/boundary/detail/NodePartition.hpp"

#ifdef TEST_BUILD
namespace loka
{
  namespace dsl
  {
    namespace testing
    {
      class SeatBuildRequestAccess;
    }
  } // namespace dsl
} // namespace loka
#endif

namespace loka
{
  namespace app
  {
    namespace scene
    {
      namespace detail
      {
        class SeatLayoutTable;
        class SeatReservations;
        class SeatBuildRequest;

        /** Stack-local node-storage access, issued only after the outgoing root's
            complete storage-owner return and a current class-capacity check.
            The synchronous build consumes its full ticket before another admission. */
        class ReturnedSeatStorage
        {
        public:
          bool build(const SeatLayoutTable &demand, NodeBuildOperation &operation);

        private:
          friend class NodePartition;
          explicit ReturnedSeatStorage(NodePartition &bank)
              : bank_(bank)
          {
          }
          ReturnedSeatStorage(const ReturnedSeatStorage &);
          ReturnedSeatStorage &operator=(const ReturnedSeatStorage &);
          NodePartition &bank_;
        };

        /** One coalescing demand in a persistent Boundary reservation. No key
            value or candidate is retained. Source and vacant child position are
            scoped borrows, canceled before scope disposal. Cancellation never ends
            the outgoing node obligation. The bank is a same-landlord borrow;
            only fixture activation currently supplies it. */
        class SeatBuildRequest
        {
        public:
          SeatBuildRequest()
              : phase_(IDLE),
                bank_(0),
                outgoing_(0),
                position_(),
                source_(0)
          {
          }
          void mark()
          {
            if (this->phase_ != CANCELLED)
              this->phase_ = WAITING;
          }
          void cancel();
          void observe(core::StateBase *source);
          void settle()
          {
            if (this->phase_ != CANCELLED)
              this->phase_ = IDLE;
          }
          bool waiting() const
          {
            return this->phase_ == WAITING;
          }
          bool enabled() const
          {
            return this->bank_ != 0;
          }
          bool retiring() const
          {
            return this->outgoing_ != 0;
          }
          /** Called at destructive retirement, before unlinking the old generation. */
          void retire(Node *outgoing)
          {
            assert(!this->outgoing_ && "a seat cannot replace a still-retiring occupant");
            if (!this->outgoing_)
              this->outgoing_ = outgoing;
            this->mark();
          }
          /** False means waiting or refusal; it never grants partial entitlement. */
          bool admit(const SeatLayoutTable &demand, NodeBuildOperation &operation);
#ifdef TEST_BUILD
          /** Borrows only a bank installed by this request's Boundary fixture. */
          void activateFixture(NodePartition &bank, core::StateBase *source = 0)
          {
            this->bank_ = &bank;
            this->observe(source);
          }
#endif
        private:
#ifdef TEST_BUILD
          friend class ::loka::dsl::testing::SeatBuildRequestAccess;
#endif
          friend class SeatReservations;
          friend class NodePartition;
          /** A vacant logical child position, owned by the surviving seat. Index
              counts current physical children; order resolves adjacent vacant seats. */
          struct Position
          {
            Position()
                : parent(0),
                  index(0),
                  order(0)
            {
            }
            Node *parent;
            size_t index;
            int order;
          };
          enum Phase
          {
            IDLE,
            WAITING,
            CANCELLED
          };
          SeatBuildRequest(const SeatBuildRequest &);
          SeatBuildRequest &operator=(const SeatBuildRequest &);
          void returned(Node *node)
          {
            if (this->outgoing_ == node)
              this->outgoing_ = 0;
          }
          Phase phase_;
          NodePartition *bank_;
          Node *outgoing_;
          Position position_;
          core::StateBase *source_;
          static void Changed(void *data)
          {
            static_cast<SeatBuildRequest *>(data)->mark();
          }
        };
      } // namespace detail
    } // namespace scene
  } // namespace app
} // namespace loka
#endif
