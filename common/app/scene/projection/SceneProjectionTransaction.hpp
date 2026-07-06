#ifndef LOKA_CORE2_SCENE_PROJECTION_SCENE_PROJECTION_TRANSACTION_HPP
#define LOKA_CORE2_SCENE_PROJECTION_SCENE_PROJECTION_TRANSACTION_HPP

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
                next(0)
          {
          }

          TargetEntry(Node *targetNode, NodeDirtyFlags flags)
              : node(targetNode),
                dirtyFlags(flags),
                next(0)
          {
          }

          void includeDirtyFlags(NodeDirtyFlags flags)
          {
            dirtyFlags = static_cast<NodeDirtyFlags>(dirtyFlags | flags);
          }

          Node *node;
          NodeDirtyFlags dirtyFlags;
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

        SceneProjectionTransaction()
            : head(0),
              tail(0)
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
            return;
          }
          entry = new TargetEntry(node, flags);
          append(entry);
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
          return head != 0;
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
            ++count;
            it.next();
          }
          return count;
        }

        void swapContents(SceneProjectionTransaction &other)
        {
          if (this == &other)
          {
            return;
          }
          TargetEntry *otherHead = other.head;
          TargetEntry *otherTail = other.tail;
          other.head = head;
          other.tail = tail;
          head = otherHead;
          tail = otherTail;
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
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_PROJECTION_SCENE_PROJECTION_TRANSACTION_HPP
