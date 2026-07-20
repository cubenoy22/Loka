#ifndef LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARY_ARENA_HPP
#define LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARY_ARENA_HPP

#include <cassert>
#include <new>
#include <vector>
#include "app/scene/Node.hpp"
#include "app/scene/detail/ArenaMath.hpp"
#include "core/LokaAlloc.hpp"
#include "core/State.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      namespace detail
      {
        // NodeArena: pre-allocated memory block for batch node creation
        class NodeArena
        {
        public:
          struct RetiredNodeGeneration
          {
            RetiredNodeGeneration()
                : buffer(0),
                  raw(0)
            {
            }

            char *buffer;
            char *raw;
            std::vector<Node *> nodes;
            /** Heap subtree roots retired alongside this generation; deleted
                first, while the ledger they may reference is still alive. */
            std::vector<Node *> heapRoots;
          };

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
              raw_ = static_cast<char *>(loka::core::LokaAllocRaw(rawSize, slabSite()));
              if (!raw_)
              {
                // Backend white flag: the arena stays empty, allocate() keeps
                // returning 0, and node creation stays on the heap path.
                // Propagating the flag to callers is #132 S3.
                return;
              }
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
              node->setArenaOwner(this);
              nodes_.push_back(node);
            }
          }

          /** Tombstones and destroys one arena node without moving the ledger. */
          bool releaseNode(Node *node)
          {
            if (!node)
            {
              return false;
            }
            assert(node->arenaOwner() == this && "node released to a foreign arena ledger");
            for (size_t i = 0; i < nodes_.size(); ++i)
            {
              if (nodes_[i] == node)
              {
                nodes_[i] = 0;
                node->~Node();
                return true;
              }
            }
            return false;
          }

          void clear()
          {
            RetiredNodeGeneration gen;
            if (detachRetiredGeneration(gen))
            {
              destroyRetiredGeneration(gen);
            }
          }

          bool detachRetiredGeneration(RetiredNodeGeneration &out)
          {
            if (!buffer_ && !raw_ && nodes_.empty())
            {
              return false;
            }

            out.buffer = buffer_;
            out.raw = raw_;
            out.nodes.swap(nodes_);
            buffer_ = 0;
            raw_ = 0;
            size_ = 0;
            offset_ = 0;
            nodes_.clear();
            return true;
          }

          static void destroyRetiredGeneration(RetiredNodeGeneration &gen)
          {
            // Heap roots go first, while every arena node they may still
            // reference is alive: their destructors skip arena children.
            for (size_t i = 0; i < gen.heapRoots.size(); ++i)
            {
              // Heap roots are non-arena by construction; DestroyHeapNode
              // routes gate storage back through the gate.
              DestroyHeapNode(gen.heapRoots[i]);
            }
            gen.heapRoots.clear();
            // Sever every parent-to-child edge while the whole ledger is alive.
            std::vector<Node *> detachedChildren;
            std::vector<Node *> detachedHeapRoots;
            for (size_t i = 0; i < gen.nodes.size(); ++i)
            {
              Node *node = gen.nodes[i];
              INestable *nestable = node ? node->asNestable() : 0;
              if (nestable)
              {
                nestable->detachChildrenTo(detachedChildren);
                for (size_t childIndex = 0; childIndex < detachedChildren.size(); ++childIndex)
                {
                  Node *child = detachedChildren[childIndex];
                  if (child && !child->isArenaAllocated())
                  {
                    detachedHeapRoots.push_back(child);
                  }
                }
              }
            }
            for (size_t i = 0; i < detachedHeapRoots.size(); ++i)
            {
              // detachedHeapRoots collected only non-arena children above.
              DestroyHeapNode(detachedHeapRoots[i]);
            }
            // Creation is parent-first, so reverse order destroys children first.
            for (size_t i = gen.nodes.size(); i > 0; --i)
            {
              Node *node = gen.nodes[i - 1];
              if (node)
              {
                node->~Node();
              }
            }
            gen.nodes.clear();
            if (gen.raw)
            {
              loka::core::LokaFreeRaw(gen.raw, slabSite());
            }
            else
            {
              delete[] gen.buffer;
            }
            gen.buffer = 0;
            gen.raw = 0;
          }

          bool hasCapacity() const
          {
            return buffer_ != 0;
          }

        private:
          /** One static gate site per slab kind: reserve() and
              destroyRetiredGeneration() must free through the tags the
              acquisition used so the audit ledger balances. */
          static const loka::core::LokaAllocationSite &slabSite()
          {
            static const loka::core::LokaAllocationSite site("NodeArena", "slab");
            return site;
          }

          char *buffer_;
          char *raw_;
          size_t size_;
          size_t offset_;
          std::vector<Node *> nodes_;
        };
      } // namespace detail

      typedef detail::NodeArena NodeArena;

      class StateArena
      {
      public:
        StateArena()
            : first_(0),
              tail_(0),
              states_()
        {
        }
        ~StateArena()
        {
          clear();
        }

        /** Ensures one owner-lifetime allocation batch has enough arena
            capacity. allocate() consumes existing capacity only; it never
            grows implicitly for unreserved node-local state. */
        void reserve(size_t totalSize)
        {
          if (totalSize == 0)
          {
            return;
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

        /** Serves allocations from the tail block only. Leftover capacity in
            earlier blocks is intentionally stranded, not tracked in a free
            list, for locality and simplicity. */
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
          return 0;
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

        bool hasBlocks() const
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
          Block *block = loka::core::LokaNew<Block>(blockSite());
          if (!block)
          {
            return false;
          }
          size_t rawSize = totalSize + kArenaAlign;
          block->raw = static_cast<char *>(loka::core::LokaAllocRaw(rawSize, slabSite()));
          if (!block->raw)
          {
            loka::core::LokaDelete(block, blockSite());
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
            loka::core::LokaFreeRaw(block->raw, slabSite());
            loka::core::LokaDelete(block, blockSite());
            block = next;
          }
          first_ = 0;
          tail_ = 0;
        }

        /** One static gate site per acquisition kind: appendBlock() and
            clearBlocks() must free through the tags the acquisition used so
            the audit ledger balances. */
        static const loka::core::LokaAllocationSite &blockSite()
        {
          static const loka::core::LokaAllocationSite site("StateArena", "Block");
          return site;
        }

        static const loka::core::LokaAllocationSite &slabSite()
        {
          static const loka::core::LokaAllocationSite site("StateArena", "slab");
          return site;
        }

        Block *first_;
        Block *tail_;
        std::vector<StateEntry> states_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_BOUNDARY_DETAIL_BOUNDARY_ARENA_HPP
