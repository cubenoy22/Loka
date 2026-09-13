#ifndef LOKA_SEAT_RESERVATION_HPP
#define LOKA_SEAT_RESERVATION_HPP

#include "app/scene/boundary/detail/SeatBuildRequest.hpp"
#include "app/scene/boundary/detail/BoundaryArena.hpp"
#include "app/scene/boundary/detail/ReclaimScratch.hpp"

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

        /** Boundary-owned immutable reservation facts and boot-once node storage.
            The partition's bounded construction quota spans cold materialization
            and the subsequent ordinary ATTACH walk. Warm admissions reset it only
            after the outgoing occupant has fully returned. */
        class SeatReservation
        {
        public:
          SeatBuildRequest &request() const { return this->request_; }
          const SeatLayoutTable &layoutTable() const
          {
            return this->table_;
          }
          NodePartition &partition() const { return this->partition_; }
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
                partition_(),
                next_(0)
          {
          }
          ~SeatReservation();
          SeatReservation(const SeatReservation &);
          SeatReservation &operator=(const SeatReservation &);
          mutable SeatBuildRequest request_;
          const SeatLayoutTable table_;
          const size_t bytes_;
          mutable NodePartition partition_;
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
          bool empty() const { return this->head_ == 0; }
          const SeatReservation *install(SeatLayoutTable table);

#ifdef TEST_BUILD
          /** Internal fixture installation using the same cold storage door. */
          NodePartition *installFixture(SeatLayoutTable table);
#endif

          /** Search only this landlord's reservations and each partition's own rows. */
          NodePartition *partitionFor(Node *node);
          bool removeSeatChild(SeatBuildRequest &request, Node *parent, Node *outgoing, int order);
          bool installSeatChild(SeatBuildRequest &request, Node *incoming);
          /** Destroys a retired generation snapshot (bounded plan when scratch is
              supplied and fits, legacy walk otherwise) and then clears the outgoing
              obligations of this landlord's requests that pointed into it. */
          void reclaimGeneration(NodeArena::RetiredNodeGeneration &generation, ReclaimScratch *scratch);
          void returnedNode(Node *node);
          void cancelRequests();
          bool hasWaitingRequests() const;
          void reclaimPartitionRoots(NodePartition::ReclaimNode reclaim, void *context);

        private:
          friend class SeatReservation;
          SeatReservations(const SeatReservations &);
          SeatReservations &operator=(const SeatReservations &);
          static void ReturnedGenerationNode(Node *node, void *owner);
          static const core::LokaAllocationSite &site();
          SeatReservation *head_;
        };

      } // namespace detail
    } // namespace scene
  } // namespace app
} // namespace loka
#endif
