#include "Boundary.hpp"
#include "app/scene/Scene.hpp"
#if defined(LOKA_DEBUG_SCENE_UPDATE) && !defined(LOKA_RETRO68)
#include "platform/debug/DebugLog.hpp"
#endif

namespace loka
{
  namespace app
  {
    namespace scene
    {
      void BoundaryNode::retireOwnedNodeGeneration()
      {
        detail::NodeArena::RetiredNodeGeneration gen;
        Node *keptHead = 0;
        Node *keptTail = 0;
        Node *retired = this->retiredSubtreesHead_;
        while (retired)
        {
          Node *next = retired->nextInComposition;
          retired->nextInComposition = 0;
          if (retired->arenaOwner() == this->nodeArena())
          {
            // Subsumed: the generation ledger already owns this corpse.
          }
          else if (retired->arenaOwner() == 0)
          {
            // Heap corpses ride the generation so their arena descendants and
            // the ledger slots those descendants occupy drain as one unit.
            gen.heapRoots.push_back(retired);
          }
          else
          {
            assert(false && "a boundary must not hold retired subtrees of a foreign arena");
            if (keptTail)
            {
              keptTail->nextInComposition = retired;
            }
            else
            {
              keptHead = retired;
            }
            keptTail = retired;
          }
          retired = next;
        }
        this->retiredSubtreesHead_ = keptHead;
        this->retiredSubtreesTail_ = keptTail;

        const bool hasArenaGeneration = this->nodeArena_.detachRetiredGeneration(gen);
        if (hasArenaGeneration || !gen.heapRoots.empty())
        {
          this->retiredGenerations_.push_back(gen);
        }

        Scene *scene = this->getScene();
        if (scene)
        {
          scene->requestRetiredSubtreeDrainAfterRun();
        }
      }

      void BoundaryNode::retireDetachedNode(ComponentContext &context, Node *node)
      {
        if (!node)
        {
          return;
        }
        Node::MarkSubtreeLifecycleFact(node, NODE_FACT_RETIRED);
        if (context.platformController())
        {
          context.platformController()->releaseNodeContexts(node);
        }
        this->retireSubtree(node);
      }

      void BoundaryNode::retireSubtree(Node *node)
      {
        if (!node)
        {
          return;
        }
        assert(node->nextInComposition == 0 &&
               "retired subtree root must be detached before reusing its sibling link");
        if (this->retiredSubtreesTail_)
        {
          this->retiredSubtreesTail_->nextInComposition = node;
        }
        else
        {
          this->retiredSubtreesHead_ = node;
        }
        this->retiredSubtreesTail_ = node;

        Scene *scene = this->getScene();
        if (scene)
        {
          scene->requestRetiredSubtreeDrainAfterRun();
        }
      }

      void BoundaryNode::destroyRetiredSubtree(Node *node)
      {
        if (!node)
        {
          return;
        }

        BoundaryNode *nestedBoundary = node->asBoundary();
        if (!nestedBoundary || nestedBoundary == this)
        {
          INestable *nestable = node->asNestable();
          if (nestable)
          {
            std::vector<Node *> children;
            nestable->detachChildrenTo(children);
            for (size_t i = 0; i < children.size(); ++i)
            {
              this->destroyRetiredSubtree(children[i]);
            }
          }
        }

        if (node->isArenaAllocated())
        {
          assert(node->arenaOwner() == this->nodeArena() &&
                 "retired arena node must belong to the retiring Boundary arena");
          const bool released = this->nodeArena_.releaseNode(node);
          assert(released && "retired arena node must belong to the retiring Boundary ledger");
        }
        else
        {
          delete node;
        }
      }

      void BoundaryNode::drainRetiredSubtreesAtNextTrackerRun()
      {
        if (this->drainingRetiredSubtrees_ ||
            (!this->retiredSubtreesHead_ && this->retiredGenerations_.empty()))
        {
          return;
        }

        Node *snapshot = this->retiredSubtreesHead_;
        this->retiredSubtreesHead_ = 0;
        this->retiredSubtreesTail_ = 0;
        std::vector<detail::NodeArena::RetiredNodeGeneration> generationSnapshot;
        generationSnapshot.swap(this->retiredGenerations_);
        this->drainingRetiredSubtrees_ = true;
        while (snapshot)
        {
          Node *next = snapshot->nextInComposition;
          snapshot->nextInComposition = 0;
          this->destroyRetiredSubtree(snapshot);
          snapshot = next;
        }
        for (size_t i = 0; i < generationSnapshot.size(); ++i)
        {
          detail::NodeArena::destroyRetiredGeneration(generationSnapshot[i]);
        }
        generationSnapshot.clear();
        this->drainingRetiredSubtrees_ = false;
      }

      void BoundaryNode::drainAllRetiredSubtrees()
      {
        while (this->retiredSubtreesHead_ || !this->retiredGenerations_.empty())
        {
          this->drainRetiredSubtreesAtNextTrackerRun();
        }
      }

      void BoundaryNode::releaseOwnedNodeStorage()
      {
        this->drainAllRetiredSubtrees();
        // Detach the owner edge before NodeArena severs and destroys its ledger.
        this->clearChildren();
        this->nodeArena_.clear();
      }

      void BoundaryNode::markViewDirty(NodeDirtyFlags flags)
      {
        if (this->isFrozen())
        {
          return;
        }
        Scene *scene = this->getScene();
        if (!scene)
        {
          return;
        }
        const bool flushImmediately = this->flushViewDirtyImmediately(flags);
        scene->requestBoundaryUpdate(this, flags, flushImmediately);
      }

      void BoundaryNode::InvalidateSceneThunk(void *userData)
      {
        BoundaryNode *self = static_cast<BoundaryNode *>(userData);
        if (!self)
        {
          return;
        }
        Scene *scene = self->getScene();
        if (scene)
        {
#if defined(LOKA_DEBUG_SCENE_UPDATE) && !defined(LOKA_RETRO68)
          loka::platform::DebugLogSceneUpdateTracked(static_cast<void *>(self), static_cast<void *>(scene));
#endif
          NodeDirtyFlags flags = self->observedDirtyFlagsForCommittedStates();
          if (flags == NODE_DIRTY_NONE)
          {
            flags = self->observedDirtyFlags();
          }
          if (flags == NODE_DIRTY_NONE)
          {
            return;
          }
          self->markViewDirty(flags);
        }
      }

      void BoundaryNode::ObservedStateChangedThunk(void *userData)
      {
        BoundaryObservedStateBinding *binding = static_cast<BoundaryObservedStateBinding *>(userData);
        if (!binding || !binding->boundary)
        {
          return;
        }
        if (binding->state && binding->state->trackerOwner() == binding->boundary->tracker())
        {
          return;
        }
        if (!binding->boundary->getScene())
        {
          return;
        }
        NodeDirtyFlags flags = binding->flags;
        if (flags == NODE_DIRTY_NONE)
        {
          return;
        }
        loka::core::StateTracker *ownerTracker = binding->state ? binding->state->trackerOwner() : 0;
        if (ownerTracker && ownerTracker->phase() != loka::core::TRACKER_IDLE)
        {
          binding->retain();
          ownerTracker->defer(&BoundaryNode::ObservedStateDeferredInvalidateThunk, binding);
          return;
        }
        binding->boundary->markViewDirty(flags);
      }

      void BoundaryNode::ObservedStateDeferredInvalidateThunk(void *userData)
      {
        BoundaryObservedStateBinding *binding = static_cast<BoundaryObservedStateBinding *>(userData);
        if (!binding)
        {
          return;
        }
        if (!binding->boundary)
        {
          if (binding->release())
          {
            delete binding;
          }
          return;
        }
        NodeDirtyFlags flags = binding->flags;
        if (flags == NODE_DIRTY_NONE)
        {
          if (binding->release())
          {
            delete binding;
          }
          return;
        }
        binding->boundary->markViewDirty(flags);
        if (binding->release())
        {
          delete binding;
        }
      }
    } // namespace scene
  } // namespace app
} // namespace loka
