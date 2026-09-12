#include "app/scene/boundary/detail/SeatReservation.hpp"
#include <new>

namespace loka
{
  namespace app
  {
    namespace scene
    {
      namespace detail
      {

        bool SeatLayoutTable::append(const NodeSlotLayout &layout)
        {
          if (this->count_ == capacity)
            return false;
          this->layouts_[this->count_++] = layout;
          return true;
        }

        bool SeatLayoutTable::normalize()
        {
          size_t normalized = 0;
          for (size_t i = 0; i < this->count_; ++i)
          {
            const NodeSlotLayout entry = this->layouts_[i];
            size_t stride = 0;
            if (!entry.count() || !entry.stride(stride))
              return false;
            size_t j = 0;
            for (; j < normalized; ++j)
              if (this->layouts_[j].size() == entry.size() && this->layouts_[j].alignment() == entry.alignment())
                break;
            if (j == normalized)
              this->layouts_[normalized++] = entry;
            else
            {
              size_t sum = 0;
              if (!NodeSlotLayout::add(this->layouts_[j].count(), entry.count(), sum))
                return false;
              this->layouts_[j] = NodeSlotLayout(entry.size(), entry.alignment(), sum);
            }
          }
          this->count_ = normalized;
          return normalized != 0;
        }

        const SeatReservation *SeatReservations::install(SeatLayoutTable table)
        {
          size_t bytes = 0;
          if (!table.normalize() || !NodePartition::reservationBytes(table.layouts(), table.count(), 0, bytes))
            return 0;
          void *storage = core::LokaAllocRaw(sizeof(SeatReservation), site());
          if (!storage)
            return 0;
          SeatReservation *reservation = new (storage) SeatReservation(table, bytes);
          reservation->next_ = this->head_;
          this->head_ = reservation;
          return reservation;
        }

        const core::LokaAllocationSite &SeatReservations::site()
        {
          static const core::LokaAllocationSite site("SeatReservation", "table");
          return site;
        }

        SeatReservations::~SeatReservations()
        {
          while (this->head_)
          {
            SeatReservation *current = this->head_;
            this->head_ = current->next_;
            current->~SeatReservation();
            core::LokaFreeRaw(current, site());
          }
        }

      } // namespace detail
    } // namespace scene
  } // namespace app
} // namespace loka
