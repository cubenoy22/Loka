#ifndef LOKA_PENDING_SUBTREE_HPP
#define LOKA_PENDING_SUBTREE_HPP

#include "app/scene/Node.hpp"
#include "app/scene/boundary/detail/BoundaryArena.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      /** Owns one unpublished root, including a partial materialization.
          The Boundary-scoped caller supplies its reclaim door; a declaration
          root defaults to release before materialization. Transfer empties the
          slot before installation or handoff to another owner. A borrowed
          reclaim context must outlive the slot. No traversal context escapes.

          Cost: construction, prepare, root and take are O(1), once per
          candidate/handoff. Reclaim/destruction invokes the supplied door once
          for a nonempty slot: declaration release visits its own subtree and
          its arena landlord's release ledger; Boundary retirement visits the
          candidate subtree and queues it on that Boundary's existing clock. */
      class PendingSubtree
      {
      public:
        typedef void (*ReclaimRoot)(Node *, void *);

        explicit PendingSubtree(ReclaimRoot reclaim = &PendingSubtree::ReleaseRoot,
                                void *context = 0)
            : root_(0), reclaim_(reclaim), context_(context)
        {
          assert(reclaim);
        }
        ~PendingSubtree()
        {
          this->reclaim();
        }
        void prepare(Node *root)
        {
          assert(!this->root_);
          this->root_ = root;
        }
        /** Install an unpublished root with its enclosing clock handoff. */
        void prepare(Node *root, ReclaimRoot reclaim, void *context)
        {
          assert(!this->root_ && reclaim);
          this->reclaim_ = reclaim;
          this->context_ = context;
          this->root_ = root;
        }
        Node *root() const
        {
          return this->root_;
        }
        Node *take()
        {
          Node *root = this->root_;
          this->root_ = 0;
          return root;
        }
        void reclaim()
        {
          if (this->root_)
            this->reclaim_(this->take(), this->context_);
        }

      private:
        static void ReleaseRoot(Node *root, void *)
        {
          IStateOwner *owner = root->asStateOwner();
          if (owner)
            owner->detachHeldResources();
          assert(!root->isPartitionAllocated() && "partition candidates require their Boundary reclaim door");
          if (root->arenaOwner())
            root->arenaOwner()->releaseNode(root);
          else
            DestroyHeapNode(root);
        }

        PendingSubtree(const PendingSubtree &);
        PendingSubtree &operator=(const PendingSubtree &);

        Node *root_;
        ReclaimRoot reclaim_;
        void *context_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka
#endif
