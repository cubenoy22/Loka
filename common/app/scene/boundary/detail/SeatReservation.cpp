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

        bool SeatLayoutTable::scale(size_t multiplicity)
        {
          if (!multiplicity) return false;
          SeatLayoutTable scaled;
          for (size_t i = 0; i < this->count_; ++i)
          {
            size_t count = 0;
            const NodeSlotLayout &layout = this->layouts_[i];
            if (!NodeSlotLayout::multiply(layout.count(), multiplicity, count))
              return false;
            if (!scaled.append(NodeSlotLayout(layout.size(), layout.alignment(), count))) return false;
          }
          *this = scaled;
          return true;
        }

        const SeatReservation *SeatReservations::install(SeatLayoutTable table, SeatReplacementPolicy policy)
        {
          size_t bytes = 0;
          if (!table.normalize()) return 0;
          SeatLayoutTable backing(table);
          switch (policy)
          {
          case RETIRE_BEFORE_BUILD: break;
          case PRESERVE_INSTALLED: if (!backing.scale(2)) return 0; break;
          }
          if (!NodePartition::reservationBytes(backing.layouts(), backing.count(), 0, bytes)) return 0;
          void *storage = core::LokaAllocRaw(sizeof(SeatReservation), site());
          if (!storage)
            return 0;
          SeatReservation *reservation = new (storage) SeatReservation(table, bytes, policy);
          if (!reservation->partition_.boot(backing.layouts(), backing.count()))
          {
            reservation->~SeatReservation();
            core::LokaFreeRaw(reservation, site());
            return 0;
          }
          reservation->request_.bank_ = &reservation->partition_;
          reservation->next_ = this->head_;
          this->head_ = reservation;
          return reservation;
        }

        SeatReservation::~SeatReservation()
        {

        }

#ifdef TEST_BUILD
        NodePartition *SeatReservations::installFixture(SeatLayoutTable table)
        {
          const SeatReservation *reservation = this->install(table);
          return reservation ? &reservation->partition() : 0;
        }
#endif

        NodePartition *SeatReservations::partitionFor(Node *node)
        {
          for (SeatReservation *r = this->head_; r; r = r->next_)
            if (r->partition_.resident(node))
              return &r->partition_;
          return 0;
        }

        void SeatReservations::reclaimPartitionRoots(NodePartition::ReclaimNode reclaim, void *context)
        {
          for (SeatReservation *r = this->head_; r; r = r->next_)
            r->partition_.reclaimRoots(reclaim, context);
        }

        bool SeatReservations::removeSeatChild(SeatBuildRequest &request, Node *parent, Node *outgoing, int order)
        {
          INestable *nestable = parent ? parent->asNestable() : 0;
          if (!nestable || !outgoing)
            return false;
          size_t index = 0;
          Node *child = nestable->childrenHead();
          for (; child && child != outgoing; child = child->nextInComposition)
            ++index;
          if (!child)
            return false;
          std::vector<Node *> children;
          nestable->detachChildrenTo(children);
          for (size_t i = 0; i < children.size(); ++i)
            if (children[i] != outgoing)
              nestable->addChild(children[i]);
          for (SeatReservation *r = this->head_; r; r = r->next_)
            if (r->request_.position_.parent == parent && r->request_.position_.index > index)
              --r->request_.position_.index;
          request.position_.parent = parent;
          request.position_.index = index;
          request.position_.order = order;
          request.recordReturn(outgoing);
          return true;
        }

        bool SeatReservations::installSeatChild(SeatBuildRequest &request, Node *incoming)
        {
          Node *parent = request.position_.parent;
          INestable *nestable = parent ? parent->asNestable() : 0;
          if (!nestable || !incoming)
            return false;
          const size_t index = request.position_.index;
          const int order = request.position_.order;
          std::vector<Node *> children;
          nestable->detachChildrenTo(children);
          for (size_t i = 0; i <= children.size(); ++i)
          {
            if (i == index)
              nestable->addChild(incoming);
            if (i < children.size())
              nestable->addChild(children[i]);
          }
          assert(index <= children.size());
          for (SeatReservation *r = this->head_; r; r = r->next_)
            if (r->request_.position_.parent == parent
                && (r->request_.position_.index > index
                    || (r->request_.position_.index == index && r->request_.position_.order > order)))
              ++r->request_.position_.index;
          request.position_ = SeatBuildRequest::Position();
          return index <= children.size();
        }

        void SeatReservations::ReturnedGenerationNode(Node *node, void *owner)
        {
          static_cast<SeatReservations *>(owner)->returnedNode(node);
        }

        void SeatReservations::reclaimGeneration(NodeArena::RetiredNodeGeneration &generation, ReclaimScratch *scratch)
        {
          if (scratch && NodeArena::destroyRetiredGeneration(generation, *scratch,
                                                            &ReturnedGenerationNode, this))
            return;
          NodeArena::destroyRetiredGeneration(generation, &ReturnedGenerationNode, this);
        }

        void SeatReservations::recordRetiringRoot(Node *node)
        {
          NodePartition *bank = node ? node->partitionOwner() : 0;
          if (!bank) return;
          for (SeatReservation *r = this->head_; r; r = r->next_)
            if (&r->partition_ == bank && r->policy_ == PRESERVE_INSTALLED)
            {
              NodePartition::Resident *resident = bank->resident(node);
              if (!resident || resident->owner) return;
              r->request_.recordReturn(node);
              return;
            }
        }

        void SeatReservations::returnedNode(Node *node)
        {
          for (SeatReservation *r = this->head_; r; r = r->next_)
            r->request_.returned(node);
        }

        void SeatReservations::resetInitialBuildRequests()
        {
          for (SeatReservation *r = this->head_; r; r = r->next_)
          {
            r->request_.cancel();
            r->request_.phase_ = SeatBuildRequest::IDLE;
          }
        }

        void SeatReservations::cancelRequests()
        {
          for (SeatReservation *r = this->head_; r; r = r->next_)
            r->request_.cancel();
        }

        bool SeatReservations::hasWaitingRequests() const
        {
          for (SeatReservation *r = this->head_; r; r = r->next_)
            if (r->request_.waiting())
              return true;
          return false;
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
