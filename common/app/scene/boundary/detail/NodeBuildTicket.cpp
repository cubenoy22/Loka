#include "app/scene/boundary/detail/NodeBuildTicket.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      namespace detail
      {

        NodeBuildTicket::NodeBuildTicket(NodePartition &partition, const SeatLayoutTable &demand)
            : partition_(partition),
              demand_(demand)
        {
          for (size_t i = 0; i < demand.count(); ++i)
            this->remaining_[i] = demand.layouts()[i].count();
        }

        void *NodeBuildTicket::consumeAndAllocate(const NodeSlotLayout &layout)
        {
          for (size_t i = 0; i < this->demand_.count(); ++i)
            if (this->demand_.layouts()[i].size() == layout.size()
                && this->demand_.layouts()[i].alignment() == layout.alignment() && this->remaining_[i])
            {
              void *storage = this->partition_.allocate(layout);
              if (storage)
                --this->remaining_[i];
              return storage;
            }
          assert(false
                 && "seat node declaration incomplete: declare every descendant layout and maximum count, including "
                    "attach children");
          return 0;
        }

        Node *NodeBuildTicket::create(NodeDefinitionBase &definition, Node *owner)
        {
          const NodeSlotLayout layout(definition.nodeSize(), definition.nodeAlign(), 1);
          void *storage = this->consumeAndAllocate(layout);
          if (!storage)
            return 0;
          Node *node = definition.createInPlace(storage);
          if (!node)
          {
            this->partition_.cancel(storage, layout);
            return 0;
          }
          if (owner && !this->partition_.resident(owner))
            owner = 0;
          const bool registered = this->partition_.registerPlaced(node, storage, layout, owner);
          assert(registered && "strict node construction must register its owner edge");
          if (!registered)
          {
            node->~Node();
            this->partition_.cancel(storage, layout);
            return 0;
          }
          return node;
        }

        bool NodePartition::buildFixture(const NodeSlotLayout *demand, size_t count, NodeBuildOperation &operation)
        {
          if (!demand || !count || count > SeatLayoutTable::capacity)
            return false;
          SeatLayoutTable table;
          for (size_t i = 0; i < count; ++i)
            if (!table.append(demand[i]))
              return false;
          if (!table.normalize())
            return false;
          for (size_t i = 0; i < table.count(); ++i)
          {
            Class *c = this->findClass(table.layouts()[i]);
            if (!c || table.layouts()[i].count() > c->count)
            {
              assert(false && "seat demand exceeds the installed node reservation");
              return false;
            }
          }
          NodeBuildTicket ticket(*this, table);
          return operation.buildAndAttach(ticket);
        }

      } // namespace detail
    } // namespace scene
  } // namespace app
} // namespace loka
