#ifndef LOKA_RECLAIM_SCRATCH_HPP
#define LOKA_RECLAIM_SCRATCH_HPP

#include "app/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      namespace detail
      {

        /** Landlord-owned, explicitly provisioned reclaim workspace. Reservation is a
            cold operation; no walk grows it. A node costs one frame plus one plan row.
            Destructors and unprovisioned legacy callers retain their existing path.
            Production reservation belongs to the seat descriptor integration (PR 4/6). */
        class ReclaimScratch
        {
          struct Frame
          {
            Node *node;
            Node *child;
            size_t dependency;
          };

        public:
          ReclaimScratch()
              : storage_(0),
                capacity_(0)
          {
          }
          ~ReclaimScratch()
          {
            core::LokaFreeRaw(this->storage_, site());
          }

          /** Once per landlord, O(bound) reserved bytes; no rows of another owner. */
          bool reserve(size_t bound)
          {
            if (this->storage_ || !bound || bound > size_t(-1) / (sizeof(Frame) + sizeof(Node *)))
              return false;
            void *storage = core::LokaAllocRaw(bound * (sizeof(Frame) + sizeof(Node *)), site());
            if (!storage)
              return false;
            this->storage_ = storage;
            this->capacity_ = bound;
            return true;
          }
          bool isReserved() const
          {
            return this->storage_ != 0;
          }

          /** A synchronous construction phase borrowing its landlord's workspace.
              append preflights only; readers consume the completed postorder after all
              appends succeed. No callbacks or tree mutation may interleave with a plan.
              Nested Boundaries are leaves: their own landlord performs their teardown.
              Dependencies are queried only from the supplying owner's resident rows. */
          class Plan
          {
          public:
            typedef Node *(*Dependency)(void *, Node *, size_t &);
            enum Edges
            {
              ALL_CHILDREN,
              HEAP_CHILDREN
            };
            Plan(ReclaimScratch &scratch, Edges edges = ALL_CHILDREN, Dependency dependency = 0, void *owner = 0)
                : scratch_(scratch),
                  count_(0),
                  edges_(edges),
                  dependency_(dependency),
                  owner_(owner)
            {
            }

            /** O(nodes + child edges), plus the owner's dependency query and duplicate
                checks. The fixed budget counts every visited node, not maximum depth.
                Overflow asserts/refuses before any node or child edge is changed. */
            bool append(Node *root)
            {
              if (!root)
                return true;
              size_t depth = 0;
              if (!this->push(root, depth))
                return false;
              while (depth)
              {
                Frame &frame = this->frames()[depth - 1];
                Node *next = frame.child;
                if (next)
                  frame.child = next->nextInComposition;
                else if (this->dependency_)
                {
                  do
                    next = this->dependency_(this->owner_, frame.node, frame.dependency);
                  while (next && this->contains(next));
                }
                if (next)
                {
                  if ((this->edges_ == HEAP_CHILDREN && next->isArenaAllocated()))
                    continue;
                  if (!this->push(next, depth))
                    return false;
                }
                else
                {
                  this->rows()[this->count_++] = frame.node;
                  --depth;
                }
              }
              return true;
            }
            /** Ledger-ordered residents already have their heap edges planned. */
            bool appendLeaf(Node *node)
            {
              if (!node)
                return true;
              size_t depth = 0;
              if (!this->push(node, depth))
                return false;
              this->rows()[this->count_++] = node;
              return true;
            }
            size_t count() const
            {
              return this->count_;
            }
            Node *node(size_t index) const
            {
              return this->rows()[index];
            }

            /** Once per completed plan, O(nodes + edges); sever while every node is
                alive, so ordinary destructors do not launch another child walk. */
            void severChildren() const
            {
              for (size_t i = 0; i < this->count_; ++i)
              {
                Node *node = this->rows()[i];
                INestable *nestable = this->traversedChildren(node);
                if (nestable)
                {
                  Node *child = nestable->detachChildren();
                  while (child)
                  {
                    Node *next = child->nextInComposition;
                    child->nextInComposition = 0;
                    child = next;
                  }
                }
              }
            }

          private:
            Plan(const Plan &);
            Plan &operator=(const Plan &);
            Frame *frames() const
            {
              return static_cast<Frame *>(this->scratch_.storage_);
            }
            Node **rows() const
            {
              return reinterpret_cast<Node **>(this->frames() + this->scratch_.capacity_);
            }
            bool contains(Node *node) const
            {
              for (size_t i = 0; i < this->count_; ++i)
                if (this->rows()[i] == node)
                  return true;
              return false;
            }
            /** Arena generation plans borrow partition roots as leaves. Their
                landlord must retain child edges until its own reclaim walk. */
            INestable *traversedChildren(Node *node) const
            {
              return node->asBoundary() || (this->edges_ == HEAP_CHILDREN && node->partitionOwner())
                         ? 0 : node->asNestable();
            }
            bool push(Node *node, size_t &depth)
            {
              if (this->count_ + depth == this->scratch_.capacity_)
              {
                assert(false
                       && "reclaim subtree exceeds declared scratch node capacity; increase the landlord reservation");
                return false;
              }
              Frame &frame = *new (this->frames() + depth++) Frame();
              frame.node = node;
              INestable *nestable = this->traversedChildren(node);
              frame.child = nestable ? nestable->childrenHead() : 0;
              frame.dependency = 0;
              return true;
            }
            ReclaimScratch &scratch_;
            size_t count_;
            Edges edges_;
            Dependency dependency_;
            void *owner_;
          };

        private:
          ReclaimScratch(const ReclaimScratch &);
          ReclaimScratch &operator=(const ReclaimScratch &);
          static const core::LokaAllocationSite &site()
          {
            static const core::LokaAllocationSite value("ReclaimScratch", "workspace");
            return value;
          }
          void *storage_;
          size_t capacity_;
        };

      } // namespace detail
    } // namespace scene
  } // namespace app
} // namespace loka
#endif
