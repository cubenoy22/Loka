#ifndef LOKA_NODE_BUILD_TICKET_HPP
#define LOKA_NODE_BUILD_TICKET_HPP
#include "app/scene/boundary/detail/SeatReservation.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      namespace detail
      {

        /** One build's aggregate layout entitlement. Noncopyable and constructible only
            inside NodePartition::buildFixture. Cancellation restores unconstructed
            storage, never the entitlement already spent on a construction attempt. */
        class NodeBuildTicket
        {
        public:
          /** The factory must return a completed T in storage, or null before beginning
              any object lifetime. Refusal occurs before invoking the factory. */
          template <class T, class Factory> T *create(Factory &factory, Node *owner = 0)
          {
            const NodeSlotLayout layout = NodeSlotLayout::of<T>(1);
            void *storage = this->consumeAndAllocate(layout);
            if (!storage)
              return 0;
            T *node = factory.construct(storage);
            if (!node)
            {
              this->partition_.cancel(storage, layout);
              return 0;
            }
            if (!this->partition_.registerNode(node, owner))
            {
              node->~T();
              this->partition_.cancel(storage, layout);
              return 0;
            }
            return node;
          }

        private:
          friend class NodePartition;
          NodeBuildTicket(NodePartition &partition, const SeatLayoutTable &demand);
          NodeBuildTicket(const NodeBuildTicket &);
          NodeBuildTicket &operator=(const NodeBuildTicket &);
          void *consumeAndAllocate(const NodeSlotLayout &layout);
          NodePartition &partition_;
          const SeatLayoutTable demand_;
          size_t remaining_[SeatLayoutTable::capacity];
        };

        /** A complete synchronous fixture build, including attach and failure cleanup.
            Returning ends entitlement; occupied slots remain charged to the partition. */
        class NodeBuildOperation
        {
        public:
          virtual ~NodeBuildOperation() {}
          virtual bool buildAndAttach(NodeBuildTicket &ticket) = 0;
        };

      } // namespace detail
    } // namespace scene
  } // namespace app
} // namespace loka
#endif
