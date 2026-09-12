#ifndef LOKA_SEAT_RESERVATION_HPP
#define LOKA_SEAT_RESERVATION_HPP

#include "app/scene/boundary/detail/SeatBuildRequest.hpp"
#include "app/scene/boundary/detail/BoundaryArena.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      namespace detail
      {

        /** Bounded installation workspace. Completed copies expose only layout facts.
            The bound applies to emitted leaves, before equal-layout normalization. */
        class SeatLayoutTable
        {
        public:
          enum
          {
            capacity = 32
          };
          SeatLayoutTable()
              : count_(0)
          {
          }
          bool append(const NodeSlotLayout &layout);
          bool normalize();
          const NodeSlotLayout *layouts() const
          {
            return this->layouts_;
          }
          size_t count() const
          {
            return this->count_;
          }

        private:
          NodeSlotLayout layouts_[capacity];
          size_t count_;
        };

        /** Boundary-lifetime immutable reservation facts, with optional owned storage.
            Only the internal fixture admission boots a partition. Production retains
            the table and its checked footprint without acquiring node backing. */
        class SeatReservation
        {
        public:
          SeatBuildRequest &request() const { return this->request_; }
          const SeatLayoutTable &layoutTable() const
          {
            return this->table_;
          }
          bool reservationBytes(size_t &out) const
          {
            out = this->bytes_;
            return true;
          }

        private:
          friend class SeatReservations;
          SeatReservation(const SeatLayoutTable &table, size_t bytes)
              : table_(table),
                bytes_(bytes),
                partition_(0),
                next_(0)
          {
          }
          ~SeatReservation();
          SeatReservation(const SeatReservation &);
          SeatReservation &operator=(const SeatReservation &);
          mutable SeatBuildRequest request_;
          const SeatLayoutTable table_;
          const size_t bytes_;
          NodePartition *partition_;
          SeatReservation *next_;
        };

        /** Owns cold installations until the enclosing Boundary is reclaimed. No
            identity lookup: the declaring instruction borrows its own installed facts. */
        class SeatReservations
        {
        public:
          SeatReservations()
              : head_(0)
          {
          }
          ~SeatReservations();
          const SeatReservation *install(SeatLayoutTable table);

#ifdef TEST_BUILD
          /** Internal reserved seat only. Production allocation remains dormant. */
          NodePartition *installFixture(SeatLayoutTable table);
#endif

          /** Search only this landlord's reservations and each partition's own rows. */
          NodePartition *partitionFor(Node *node);
          bool removeSeatChild(SeatBuildRequest &request, Node *parent, Node *outgoing, int order);
          bool installSeatChild(SeatBuildRequest &request, Node *incoming);
          void reclaimGeneration(NodeArena::RetiredNodeGeneration &generation);
          void returnedNode(Node *node);
          void cancelRequests();
          bool hasWaitingRequests() const;
          void reclaimPartitionRoots(NodePartition::ReclaimNode reclaim, void *context);

        private:
          friend class SeatReservation;
          SeatReservations(const SeatReservations &);
          SeatReservations &operator=(const SeatReservations &);
          static const core::LokaAllocationSite &site();
          SeatReservation *head_;
        };

      } // namespace detail
    } // namespace scene
  } // namespace app
} // namespace loka
#endif
