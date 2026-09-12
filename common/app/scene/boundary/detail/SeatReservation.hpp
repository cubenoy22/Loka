#ifndef LOKA_SEAT_RESERVATION_HPP
#define LOKA_SEAT_RESERVATION_HPP

#include "app/scene/boundary/detail/NodePartition.hpp"

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

        /** Boundary-lifetime immutable reservation facts. Production retains only this
            table and its checked footprint; no node backing is acquired. */
        class SeatReservation
        {
        public:
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
                next_(0)
          {
          }
          SeatReservation(const SeatReservation &);
          SeatReservation &operator=(const SeatReservation &);
          const SeatLayoutTable table_;
          const size_t bytes_;
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

        private:
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
