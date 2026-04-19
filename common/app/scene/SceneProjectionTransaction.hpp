#ifndef LOKA_CORE2_SCENE_SCENE_PROJECTION_TRANSACTION_HPP
#define LOKA_CORE2_SCENE_SCENE_PROJECTION_TRANSACTION_HPP

#include "app/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      struct SceneProjectionTransaction
      {
        struct TargetEntry
        {
          TargetEntry()
              : node(0), dirtyFlags(NODE_DIRTY_NONE), next(0)
          {
          }

          Node *node;
          NodeDirtyFlags dirtyFlags;
          TargetEntry *next;
        };

        SceneProjectionTransaction()
            : head(0), tail(0)
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
            entry->dirtyFlags = static_cast<NodeDirtyFlags>(entry->dirtyFlags | flags);
            return;
          }
          entry = new TargetEntry();
          entry->node = node;
          entry->dirtyFlags = flags;
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
          TargetEntry *entry = head;
          while (entry)
          {
            flags = static_cast<NodeDirtyFlags>(flags | entry->dirtyFlags);
            entry = entry->next;
          }
          return flags;
        }

        TargetEntry *targetsHead() const
        {
          return head;
        }

      private:
        TargetEntry *find(Node *node) const
        {
          TargetEntry *entry = head;
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

        TargetEntry *head;
        TargetEntry *tail;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_SCENE_PROJECTION_TRANSACTION_HPP
