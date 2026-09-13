#include "app/scene/boundary/detail/NodeBuildTicket.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      namespace detail
      {
        NodePartition::BuildCapacity NodePartition::buildCapacity(const NodeSlotLayout *demand, size_t count) const
        {
          bool available = true;
          for (size_t i = 0; i < count; ++i)
          {
            const Class *found = 0;
            for (size_t c = 0; c < this->classCount_; ++c)
              if (this->classes_[c].size == demand[i].size() && this->classes_[c].alignment == demand[i].alignment())
                found = this->classes_ + c;
            if (!found || demand[i].count() > found->count)
              return BUILD_OVERFLOW;
            size_t free = 0;
            void *slot = found->head;
            while (slot && free < demand[i].count())
            {
              ++free;
              std::memcpy(&slot, slot, sizeof(void *));
            }
            if (free < demand[i].count())
              available = false;
          }
          return available ? BUILD_AVAILABLE : BUILD_WAIT;
        }

        bool ReturnedSeatStorage::build(const SeatLayoutTable &demand, NodeBuildOperation &operation)
        {
          return this->bank_.build(demand.layouts(), demand.count(), operation);
        }

        void SeatBuildRequest::observe(core::StateBase *source)
        {
          if (!this->bank_ || this->phase_ == CANCELLED || source == this->source_)
            return;
          if (this->source_)
            this->source_->unbind(&Changed, this);
          this->source_ = source;
          if (this->source_)
            this->source_->bind(&Changed, this, false);
        }

        void SeatBuildRequest::cancel()
        {
          this->phase_ = CANCELLED;
          this->position_ = Position();
          if (this->source_)
            this->source_->unbind(&Changed, this);
          this->source_ = 0;
        }

        bool SeatBuildRequest::admit(const SeatLayoutTable &demand, NodeBuildOperation &operation)
        {
          return this->waiting() && this->bank_ && this->bank_->admitReturned(*this, demand, operation);
        }

        bool NodePartition::admitReturned(SeatBuildRequest &request,
                                          const SeatLayoutTable &demand,
                                          NodeBuildOperation &operation)
        {
          const BuildCapacity capacity = this->buildCapacity(demand.layouts(), demand.count());
          if (capacity == BUILD_OVERFLOW)
          {
            assert(false && "seat demand exceeds its declared node bank");
            request.settle();
            return false;
          }
          if (request.retiring() || capacity == BUILD_WAIT)
            return false;
          request.settle();
          ReturnedSeatStorage returned(*this);
          if (returned.build(demand, operation))
            return true;
          request.mark();
          return false;
        }
      } // namespace detail
    } // namespace scene
  } // namespace app
} // namespace loka
