#ifndef LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARY_ARENA_HPP
#define LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARY_ARENA_HPP

#include <vector>
#include "app/scene/Node.hpp"
#include "app/scene/detail/ArenaMath.hpp"
#include "core/State.hpp"

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
        NodeArena()
            : buffer_(0),
              raw_(0),
              size_(0),
              offset_(0)
        {
        }
        ~NodeArena()
        {
          clear();
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
          align = detail::NormalizeArenaAlign(align);
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

        bool hasCapacity() const
        {
          return buffer_ != 0;
        }

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
        StateArena()
            : first_(0),
              tail_(0),
              growthHint_(0),
              states_()
        {
        }
        ~StateArena()
        {
          clear();
        }

        void reserve(size_t totalSize)
        {
          if (totalSize == 0)
          {
            return;
          }
          if (totalSize > growthHint_)
          {
            growthHint_ = totalSize;
          }
          if (!tail_)
          {
            appendBlock(totalSize);
            return;
          }
          if (remainingCapacity(*tail_) < totalSize)
          {
            appendBlock(totalSize);
          }
        }

        void *allocate(size_t size, size_t align)
        {
          if (size == 0)
          {
            return 0;
          }
          if (!tail_)
          {
            return 0;
          }
          void *ptr = allocateFromBlock(*tail_, size, align);
          if (ptr)
          {
            return ptr;
          }
          size_t blockSize = growthHint_;
          size_t required = size + detail::NormalizeArenaAlign(align);
          if (blockSize < required)
          {
            blockSize = required;
          }
          if (!appendBlock(blockSize))
          {
            return 0;
          }
          return allocateFromBlock(*tail_, size, align);
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
          clearBlocks();
        }

        bool hasCapacity() const
        {
          return tail_ != 0;
        }

      private:
        struct Block
        {
          Block()
              : buffer(0),
                raw(0),
                size(0),
                offset(0),
                next(0)
          {
          }

          char *buffer;
          char *raw;
          size_t size;
          size_t offset;
          Block *next;
        };

        struct StateEntry
        {
          StateEntry()
              : state(0),
                destroy(0)
          {
          }
          loka::core::StateBase *state;
          void (*destroy)(loka::core::StateBase *);
        };

        static size_t remainingCapacity(const Block &block)
        {
          return block.offset < block.size ? (block.size - block.offset) : 0;
        }

        static void *allocateFromBlock(Block &block, size_t size, size_t align)
        {
          if (!block.buffer)
          {
            return 0;
          }
          align = detail::NormalizeArenaAlign(align);
          size_t mask = align - 1;
          size_t aligned = (block.offset + mask) & ~mask;
          if (aligned + size > block.size)
          {
            return 0;
          }
          void *ptr = block.buffer + aligned;
          block.offset = aligned + size;
          return ptr;
        }

        bool appendBlock(size_t totalSize)
        {
          const size_t kArenaAlign = 16;
          Block *block = new Block();
          if (!block)
          {
            return false;
          }
          size_t rawSize = totalSize + kArenaAlign;
          block->raw = new char[rawSize];
          if (!block->raw)
          {
            delete block;
            return false;
          }
          size_t rawAddr = reinterpret_cast<size_t>(block->raw);
          size_t alignedAddr = (rawAddr + (kArenaAlign - 1)) & ~(kArenaAlign - 1);
          block->buffer = reinterpret_cast<char *>(alignedAddr);
          block->size = rawSize - (alignedAddr - rawAddr);
          if (!first_)
          {
            first_ = block;
          }
          else
          {
            tail_->next = block;
          }
          tail_ = block;
          return true;
        }

        void clearBlocks()
        {
          Block *block = first_;
          while (block)
          {
            Block *next = block->next;
            delete[] block->raw;
            delete block;
            block = next;
          }
          first_ = 0;
          tail_ = 0;
          growthHint_ = 0;
        }

        Block *first_;
        Block *tail_;
        size_t growthHint_;
        std::vector<StateEntry> states_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARY_ARENA_HPP
