#ifndef LOKA_CORE2_SCENE_PROJECTION_SCENE_PROJECTION_TRANSACTION_HPP
#define LOKA_CORE2_SCENE_PROJECTION_SCENE_PROJECTION_TRANSACTION_HPP

#include <vector>
#include "app/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      struct SceneProjectionTransaction
      {
        struct TargetIdentity
        {
          TargetIdentity()
              : address(0)
          {
          }

          explicit TargetIdentity(const void *value)
              : address(value)
          {
          }

          bool isValid() const
          {
            return address != 0;
          }

          const void *address;
        };

      private:
        struct TargetEntry
        {
          TargetEntry()
              : node(0),
                dirtyFlags(NODE_DIRTY_NONE),
                epoch(0),
                next(0)
          {
          }

          TargetEntry(Node *targetNode, NodeDirtyFlags flags)
              : node(targetNode),
                dirtyFlags(flags),
                epoch(0),
                next(0)
          {
          }

          void includeDirtyFlags(NodeDirtyFlags flags)
          {
            dirtyFlags = static_cast<NodeDirtyFlags>(dirtyFlags | flags);
          }

          Node *node;
          NodeDirtyFlags dirtyFlags;
          unsigned long epoch;
          TargetEntry *next;
        };

      public:
        class ConstIterator
        {
        public:
          ConstIterator()
              : entry_(0)
          {
          }

          bool isValid() const
          {
            return entry_ != 0;
          }

          void next()
          {
            if (entry_)
            {
              entry_ = entry_->next;
            }
          }

          Node *node() const
          {
            return entry_ ? entry_->node : 0;
          }

          NodeDirtyFlags dirtyFlags() const
          {
            return entry_ ? entry_->dirtyFlags : NODE_DIRTY_NONE;
          }

        private:
          friend struct SceneProjectionTransaction;

          explicit ConstIterator(const TargetEntry *entry)
              : entry_(entry)
          {
          }

          const TargetEntry *entry_;
        };

        // A live target carried out of takeUnconsumed() so the caller can
        // requeue it through the normal enqueue path after a clear.
        struct CarriedTarget
        {
          CarriedTarget()
              : node(0),
                dirtyFlags(NODE_DIRTY_NONE)
          {
          }

          CarriedTarget(Node *targetNode, NodeDirtyFlags flags)
              : node(targetNode),
                dirtyFlags(flags)
          {
          }

          Node *node;
          NodeDirtyFlags dirtyFlags;
        };

        SceneProjectionTransaction()
            : head(0),
              tail(0),
              epoch_(0)
        {
        }

        ~SceneProjectionTransaction()
        {
          clear();
        }

        void enqueue(Node *node, NodeDirtyFlags flags)
        {
          if (!node)
          {
            return;
          }
          if (flags == NODE_DIRTY_NONE)
          {
            flags = NODE_DIRTY_PROPS;
          }
          TargetEntry *entry = find(node);
          if (entry)
          {
            entry->includeDirtyFlags(flags);
            // A coalescing write re-arms the entry: it must survive the
            // end-of-cycle clear of the window it lands in.
            entry->epoch = epoch_;
            return;
          }
          entry = new TargetEntry(node, flags);
          entry->epoch = epoch_;
          append(entry);
        }

        // Marks the start of an apply window: entries stamped before this
        // point are the ones the in-flight cycle consumed; anything enqueued
        // (or re-armed by coalescing) afterwards belongs to the next cycle.
        void beginApplyWindow()
        {
          ++epoch_;
        }

        // Collects the live targets enqueued at or after the current apply
        // window (tombstones excluded) so the caller can clear the
        // transaction and requeue them through the normal enqueue path.
        void takeUnconsumed(std::vector<CarriedTarget> &out) const
        {
          const TargetEntry *entry = head;
          while (entry)
          {
            if (entry->node && entry->epoch == epoch_)
            {
              out.push_back(CarriedTarget(entry->node, entry->dirtyFlags));
            }
            entry = entry->next;
          }
        }

        // Drops a target without unlinking: the entry is tombstoned (node = 0,
        // dirtyFlags = NODE_DIRTY_NONE) so removal is safe while a
        // ConstIterator is live. Tombstones are reclaimed by clear() at the
        // end of the update cycle.
        bool removeTarget(const Node *node)
        {
          if (!node)
          {
            return false;
          }
          TargetEntry *entry = const_cast<TargetEntry *>(find(node));
          if (!entry)
          {
            return false;
          }
          entry->node = 0;
          entry->dirtyFlags = NODE_DIRTY_NONE;
          return true;
        }

        void clear()
        {
          TargetEntry *entry = head;
          while (entry)
          {
            TargetEntry *next = entry->next;
            delete entry;
            entry = next;
          }
          head = 0;
          tail = 0;
        }

        bool hasPending() const
        {
          const TargetEntry *entry = head;
          while (entry)
          {
            if (entry->node)
            {
              return true;
            }
            entry = entry->next;
          }
          return false;
        }

        NodeDirtyFlags aggregateDirtyFlags() const
        {
          NodeDirtyFlags flags = NODE_DIRTY_NONE;
          const TargetEntry *entry = head;
          while (entry)
          {
            flags = static_cast<NodeDirtyFlags>(flags | entry->dirtyFlags);
            entry = entry->next;
          }
          return flags;
        }

        NodeDirtyFlags dirtyFlagsForNode(const Node *node) const
        {
          const TargetEntry *entry = find(node);
          return entry ? entry->dirtyFlags : NODE_DIRTY_NONE;
        }

        NodeDirtyFlags dirtyFlagsForTargetIdentity(TargetIdentity identity) const
        {
          if (!identity.isValid())
          {
            return NODE_DIRTY_NONE;
          }
          const TargetEntry *entry = head;
          while (entry)
          {
            if (static_cast<const void *>(entry->node) == identity.address)
            {
              return entry->dirtyFlags;
            }
            entry = entry->next;
          }
          return NODE_DIRTY_NONE;
        }

        ConstIterator targetsBegin() const
        {
          return ConstIterator(head);
        }

        long targetCount() const
        {
          long count = 0;
          ConstIterator it = targetsBegin();
          while (it.isValid())
          {
            if (it.node())
            {
              ++count;
            }
            it.next();
          }
          return count;
        }

      private:
        SceneProjectionTransaction(const SceneProjectionTransaction &);
        SceneProjectionTransaction &operator=(const SceneProjectionTransaction &);

        TargetEntry *find(Node *node) const
        {
          return const_cast<TargetEntry *>(find(static_cast<const Node *>(node)));
        }

        const TargetEntry *find(const Node *node) const
        {
          const TargetEntry *entry = head;
          while (entry)
          {
            if (entry->node == node)
            {
              return entry;
            }
            entry = entry->next;
          }
          return 0;
        }

        void append(TargetEntry *entry)
        {
          if (!entry)
          {
            return;
          }
          if (!head)
          {
            head = entry;
            tail = entry;
          }
          else
          {
            tail->next = entry;
            tail = entry;
          }
        }

        TargetEntry *head;
        TargetEntry *tail;
        unsigned long epoch_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_PROJECTION_SCENE_PROJECTION_TRANSACTION_HPP
