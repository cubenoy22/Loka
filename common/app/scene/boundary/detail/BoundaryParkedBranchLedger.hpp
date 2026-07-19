#ifndef LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARYPARKEDBRANCHLEDGER_HPP
#define LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARYPARKEDBRANCHLEDGER_HPP

#include <vector>
#include "app/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      struct BoundaryParkedBranchKey
      {
        BoundaryParkedBranchKey(NodeTag tagValue = NODE_TAG_NONE,
                                int slotValue = -1,
                                const void *propsTypeValue = 0)
            : tag(tagValue),
              slot(slotValue),
              propsTypeId(propsTypeValue)
        {
        }

        bool matches(const BoundaryParkedBranchKey &other) const
        {
          const bool sameSeat = this->tag != NODE_TAG_NONE
                                    ? this->tag == other.tag
                                    : other.tag == NODE_TAG_NONE && this->slot == other.slot;
          return sameSeat && this->propsTypeId == other.propsTypeId;
        }

        NodeTag tag;
        int slot;
        const void *propsTypeId;
      };

      /** Boundary-owned enumeration and exact-match storage for parked branches. */
      class BoundaryParkedBranchLedger
      {
      public:
        struct Entry
        {
          Entry(const BoundaryParkedBranchKey &keyValue,
                Node *branchValue,
                bool conditionValue)
              : key(keyValue),
                branch(branchValue),
                condition(conditionValue)
          {
          }

          BoundaryParkedBranchKey key;
          Node *branch;
          bool condition;
        };

        void park(const BoundaryParkedBranchKey &key, Node *branch, bool condition)
        {
          if (branch)
          {
            this->entries_.push_back(Entry(key, branch, condition));
          }
        }

        Node *take(const BoundaryParkedBranchKey &key)
        {
          bool condition = false;
          return this->take(key, condition);
        }

        Node *take(const BoundaryParkedBranchKey &key, bool &condition)
        {
          for (size_t i = 0; i < this->entries_.size(); ++i)
          {
            if (!this->entries_[i].key.matches(key))
            {
              continue;
            }
            Node *branch = this->entries_[i].branch;
            condition = this->entries_[i].condition;
            this->entries_.erase(this->entries_.begin() + i);
            return branch;
          }
          return 0;
        }

        Node *branch(unsigned index) const
        {
          return index < this->entries_.size() ? this->entries_[index].branch : 0;
        }

        Entry *entry(unsigned index)
        {
          return index < this->entries_.size() ? &this->entries_[index] : 0;
        }

        const Entry *entry(unsigned index) const
        {
          return index < this->entries_.size() ? &this->entries_[index] : 0;
        }

        void detachAll(std::vector<Node *> &out)
        {
          out.clear();
          out.reserve(this->entries_.size());
          for (size_t i = 0; i < this->entries_.size(); ++i)
          {
            out.push_back(this->entries_[i].branch);
          }
          this->entries_.clear();
        }

      private:
        std::vector<Entry> entries_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARYPARKEDBRANCHLEDGER_HPP
