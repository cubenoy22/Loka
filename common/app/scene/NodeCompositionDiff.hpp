#ifndef LOKA_CORE2_SCENE_NODECOMPOSITIONDIFF_HPP
#define LOKA_CORE2_SCENE_NODECOMPOSITIONDIFF_HPP

#include "app/scene/Node.hpp"
#include "loka/dsl/CompositionDiff.hpp"
#include "loka/dsl/CompositionList.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      struct NodeCompositionDiff : public loka::dsl::CompositionDiff
      {
        enum Action
        {
          ACTION_RETAIN = 0,
          ACTION_REPLACE = 1,
          ACTION_RETIRE = 2
        };

        struct Entry
        {
          Entry()
              : tag(NODE_TAG_NONE),
                slot(-1),
                action(ACTION_RETAIN),
                compatibleType(false),
                equivalentProps(false),
                previousIndex(-1),
                currentIndex(-1),
                nextInComposition(0)
          {
          }

          Entry(NodeTag tagValue,
                int slotValue,
                Action actionValue,
                bool compatibleTypeValue,
                bool equivalentPropsValue,
                int previousIndexValue,
                int currentIndexValue)
              : tag(tagValue),
                slot(slotValue),
                action(actionValue),
                compatibleType(compatibleTypeValue),
                equivalentProps(equivalentPropsValue),
                previousIndex(previousIndexValue),
                currentIndex(currentIndexValue),
                nextInComposition(0)
          {
          }

          Entry(const Entry &other)
              : tag(other.tag),
                slot(other.slot),
                action(other.action),
                compatibleType(other.compatibleType),
                equivalentProps(other.equivalentProps),
                previousIndex(other.previousIndex),
                currentIndex(other.currentIndex),
                nextInComposition(0)
          {
          }

          Entry &operator=(const Entry &other)
          {
            if (this == &other)
            {
              return *this;
            }
            tag = other.tag;
            slot = other.slot;
            action = other.action;
            compatibleType = other.compatibleType;
            equivalentProps = other.equivalentProps;
            previousIndex = other.previousIndex;
            currentIndex = other.currentIndex;
            nextInComposition = 0;
            return *this;
          }

          Entry *clone() const { return new Entry(*this); }

          NodeTag tag;
          int slot;
          Action action;
          bool compatibleType;
          bool equivalentProps;
          int previousIndex;
          int currentIndex;
          Entry *nextInComposition;
        };

        NodeCompositionDiff() : loka::dsl::CompositionDiff(), entries() {}

        void clear()
        {
          loka::dsl::CompositionDiff::clear();
          entries.clear();
        }

        void addEntry(NodeTag tag,
                      int slot,
                      Action action,
                      bool compatibleType,
                      bool equivalentProps,
                      int previousIndex,
                      int currentIndex)
        {
          entries.appendOwned(new Entry(tag, slot, action, compatibleType, equivalentProps, previousIndex, currentIndex));
        }

        bool empty() const { return entries.count() == 0; }
        size_t entryCount() const { return entries.count(); }
        Entry *entriesHead() const { return entries.head(); }
        bool hasIncompatibleRetain() const
        {
          // A retained slot/tag is only safe for local apply when the retained
          // child still resolves to a compatible definition type. If the type
          // changed under the same tag, callers must fall back to rebuild.
          for (Entry *entry = entries.head(); entry; entry = entry->nextInComposition)
          {
            if (entry->action == ACTION_RETAIN && !entry->compatibleType)
            {
              return true;
            }
          }
          return false;
        }
        bool isStableRetainOnly() const
        {
          if (empty())
          {
            return false;
          }
          for (Entry *entry = entries.head(); entry; entry = entry->nextInComposition)
          {
            if (entry->action != ACTION_RETAIN)
            {
              return false;
            }
            if (!entry->compatibleType || !entry->equivalentProps)
            {
              return false;
            }
            if (entry->previousIndex != entry->currentIndex)
            {
              return false;
            }
          }
          return true;
        }
        bool isCompatibleRetainOnly() const
        {
          if (empty())
          {
            return false;
          }
          for (Entry *entry = entries.head(); entry; entry = entry->nextInComposition)
          {
            if (entry->action != ACTION_RETAIN)
            {
              return false;
            }
            if (!entry->compatibleType)
            {
              return false;
            }
            if (entry->previousIndex != entry->currentIndex)
            {
              return false;
            }
          }
          return true;
        }
        bool isCompatibleRetainOrReplaceOnly() const
        {
          if (empty())
          {
            return false;
          }
          for (Entry *entry = entries.head(); entry; entry = entry->nextInComposition)
          {
            if (entry->action == ACTION_RETIRE)
            {
              return false;
            }
            if (entry->action == ACTION_RETAIN && !entry->compatibleType)
            {
              return false;
            }
            if (entry->previousIndex != entry->currentIndex)
            {
              return false;
            }
          }
          return true;
        }

        loka::dsl::CompositionList<Entry> entries;
      };
    }
  }
}

#endif // LOKA_CORE2_SCENE_NODECOMPOSITIONDIFF_HPP
