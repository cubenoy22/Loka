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
                previousIndex(-1),
                currentIndex(-1),
                nextInComposition(0)
          {
          }

          Entry(NodeTag tagValue, int slotValue, Action actionValue, int previousIndexValue, int currentIndexValue)
              : tag(tagValue),
                slot(slotValue),
                action(actionValue),
                previousIndex(previousIndexValue),
                currentIndex(currentIndexValue),
                nextInComposition(0)
          {
          }

          Entry(const Entry &other)
              : tag(other.tag),
                slot(other.slot),
                action(other.action),
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
            previousIndex = other.previousIndex;
            currentIndex = other.currentIndex;
            nextInComposition = 0;
            return *this;
          }

          Entry *clone() const { return new Entry(*this); }

          NodeTag tag;
          int slot;
          Action action;
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

        void addEntry(NodeTag tag, int slot, Action action, int previousIndex, int currentIndex)
        {
          entries.appendOwned(new Entry(tag, slot, action, previousIndex, currentIndex));
        }

        bool empty() const { return entries.count() == 0; }
        size_t entryCount() const { return entries.count(); }
        Entry *entriesHead() const { return entries.head(); }

        loka::dsl::CompositionList<Entry> entries;
      };
    }
  }
}

#endif // LOKA_CORE2_SCENE_NODECOMPOSITIONDIFF_HPP
