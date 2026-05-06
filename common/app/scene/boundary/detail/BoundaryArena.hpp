#ifndef LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARY_ARENA_HPP
#define LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARY_ARENA_HPP

#include <vector>
#include "app/scene/Node.hpp"
#include "loka/core/State.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      // NodeArena: pre-allocated memory block for batch node creation
      class NodeArena
      {
      public:
        NodeArena() : buffer_(0), raw_(0), size_(0), offset_(0) {}
        ~NodeArena() { clear(); }

        static size_t normalizeAlign(size_t align)
        {
          size_t minAlign = sizeof(void *);
          if (minAlign < 2)
          {
            minAlign = 2;
          }
          if (align < minAlign)
          {
            align = minAlign;
          }
          if ((align & (align - 1)) != 0)
          {
            size_t p2 = 1;
            while (p2 < align)
            {
              p2 <<= 1;
            }
            align = p2;
          }
          return align;
        }

        void reserve(size_t totalSize)
        {
          clear();
          if (totalSize > 0)
          {
            // Allocate with extra padding and align the base pointer.
            const size_t kArenaAlign = 16;
            size_t rawSize = totalSize + kArenaAlign;
            raw_ = new char[rawSize];
            size_t rawAddr = reinterpret_cast<size_t>(raw_);
            size_t alignedAddr = (rawAddr + (kArenaAlign - 1)) & ~(kArenaAlign - 1);
            buffer_ = reinterpret_cast<char *>(alignedAddr);
            size_ = rawSize - (alignedAddr - rawAddr);
            offset_ = 0;
          }
        }

        void *allocate(size_t size, size_t align)
        {
          if (!buffer_ || size == 0)
          {
            return 0;
          }
          align = normalizeAlign(align);
          // Align the offset
          size_t mask = align - 1;
          size_t aligned = (offset_ + mask) & ~mask;
          if (aligned + size > size_)
          {
            return 0; // Arena full
          }
          void *ptr = buffer_ + aligned;
          offset_ = aligned + size;
          return ptr;
        }

        void registerNode(Node *node)
        {
          if (node)
          {
            nodes_.push_back(node);
          }
        }

        void clear()
        {
          // Call destructors in creation order so parents can safely detach children.
          for (size_t i = 0; i < nodes_.size(); ++i)
          {
            nodes_[i]->~Node();
          }
          nodes_.clear();
          if (raw_)
          {
            delete[] raw_;
          }
          else
          {
            delete[] buffer_;
          }
          buffer_ = 0;
          raw_ = 0;
          size_ = 0;
          offset_ = 0;
        }

        bool hasCapacity() const { return buffer_ != 0; }

      private:
        char *buffer_;
        char *raw_;
        size_t size_;
        size_t offset_;
        std::vector<Node *> nodes_;
      };

      class StateArena
      {
      public:
        StateArena() : buffer_(0), raw_(0), size_(0), offset_(0), states_() {}
        ~StateArena() { clear(); }

        void reserve(size_t totalSize)
        {
          if (buffer_ || totalSize == 0)
          {
            return;
          }
          if (totalSize > 0)
          {
            const size_t kArenaAlign = 16;
            size_t rawSize = totalSize + kArenaAlign;
            raw_ = new char[rawSize];
            size_t rawAddr = reinterpret_cast<size_t>(raw_);
            size_t alignedAddr = (rawAddr + (kArenaAlign - 1)) & ~(kArenaAlign - 1);
            buffer_ = reinterpret_cast<char *>(alignedAddr);
            size_ = rawSize - (alignedAddr - rawAddr);
            offset_ = 0;
          }
        }

        void *allocate(size_t size, size_t align)
        {
          if (!buffer_ || size == 0)
          {
            return 0;
          }
          align = NodeArena::normalizeAlign(align);
          size_t mask = align - 1;
          size_t aligned = (offset_ + mask) & ~mask;
          if (aligned + size > size_)
          {
            return 0;
          }
          void *ptr = buffer_ + aligned;
          offset_ = aligned + size;
          return ptr;
        }

        void registerState(loka::core::StateBase *state, void (*destroy)(loka::core::StateBase *))
        {
          if (state && destroy)
          {
            StateEntry entry;
            entry.state = state;
            entry.destroy = destroy;
            states_.push_back(entry);
          }
        }

        void releaseState(loka::core::StateBase *state)
        {
          if (!state)
          {
            return;
          }
          for (size_t i = 0; i < states_.size();)
          {
            if (states_[i].state == state)
            {
              if (states_[i].destroy)
              {
                states_[i].destroy(states_[i].state);
              }
              states_.erase(states_.begin() + i);
            }
            else
            {
              ++i;
            }
          }
        }

        void clear()
        {
          for (size_t i = 0; i < states_.size(); ++i)
          {
            if (states_[i].destroy)
            {
              states_[i].destroy(states_[i].state);
            }
          }
          states_.clear();
          if (raw_)
          {
            delete[] raw_;
          }
          else
          {
            delete[] buffer_;
          }
          buffer_ = 0;
          raw_ = 0;
          size_ = 0;
          offset_ = 0;
        }

        bool hasCapacity() const { return buffer_ != 0; }

      private:
        struct StateEntry
        {
          StateEntry() : state(0), destroy(0) {}
          loka::core::StateBase *state;
          void (*destroy)(loka::core::StateBase *);
        };
        char *buffer_;
        char *raw_;
        size_t size_;
        size_t offset_;
        std::vector<StateEntry> states_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARY_ARENA_HPP
