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

        /** Stack-local access to the partition's bounded admission quota.
            Cold ATTACH resumes that quota in normal sibling order; a warm build
            keeps one access through materialization and ATTACH. Cancellation
            restores storage, never quota spent on a construction attempt. */
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

          Node *create(NodeDefinitionBase &definition, Node *owner);
          NodePartition &partition() const { return this->partition_; }

        private:
          friend class NodePartition;
          friend class ::loka::app::scene::BoundaryNode;
          explicit NodeBuildTicket(NodePartition &partition) : partition_(partition) {}
          NodeBuildTicket(const NodeBuildTicket &);
          NodeBuildTicket &operator=(const NodeBuildTicket &);
          void *consumeAndAllocate(const NodeSlotLayout &layout);
          NodePartition &partition_;

        };

        /** Borrowed strict route for one synchronous generation build. */
        class SeatNodeStorageView
        {
        public:
          explicit SeatNodeStorageView(NodeBuildTicket &ticket) : ticket_(ticket) {}
          Node *create(NodeDefinitionBase &definition, Node *owner)
          { return this->ticket_.create(definition, owner); }
          bool uses(NodePartition *partition) const { return &this->ticket_.partition() == partition; }
        private:
          SeatNodeStorageView(const SeatNodeStorageView &);
          SeatNodeStorageView &operator=(const SeatNodeStorageView &);
          NodeBuildTicket &ticket_;
        };

        /** Synchronous admitted construction. Warm replacement includes ATTACH;
            cold installation resumes its owned quota during normal tree ATTACH.
            Occupied slots remain charged to the partition until reclamation. */
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
