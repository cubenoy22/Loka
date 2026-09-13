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
          if (!reservation->partition_.boot(table.layouts(), table.count()))
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
          request.retire(outgoing);
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
          // Snapshot identities before the arena clears its rows. This scratch
          // borrows only this landlord's requests and ends before its reclamation.
          std::vector<SeatBuildRequest *> completed;
          for (SeatReservation *r = this->head_; r; r = r->next_)
          {
            Node *outgoing = r->request_.outgoing_;
            if (!outgoing)
              continue;
            for (size_t i = 0; i < generation.nodes.size(); ++i)
              if (generation.nodes[i] == outgoing)
                completed.push_back(&r->request_);
            for (size_t i = 0; i < generation.heapRoots.size(); ++i)
              if (generation.heapRoots[i] == outgoing)
                completed.push_back(&r->request_);
          }
          NodeArena::destroyRetiredGeneration(generation);
          for (size_t i = 0; i < completed.size(); ++i)
            completed[i]->returned(completed[i]->outgoing_);
        }

        void SeatReservations::returnedNode(Node *node)
        {
          for (SeatReservation *r = this->head_; r; r = r->next_)
            r->request_.returned(node);
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
