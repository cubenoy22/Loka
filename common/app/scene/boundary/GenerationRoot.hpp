#ifndef LOKA_GENERATION_ROOT_HPP
#define LOKA_GENERATION_ROOT_HPP

#include "app/scene/boundary/Boundary.hpp"
#include "app/scene/boundary/PendingSubtree.hpp"
#include "app/scene/boundary/BoundaryInnerStateOwner.hpp"
#include "app/scene/node/ComposableNode.hpp"
#include "app/scene/boundary/detail/BranchSeatDeclaration.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      enum LazyScopeStatus
      {
        LAZY_SCOPE_READY,
        LAZY_SCOPE_STATES_REFUSED
      };

      /** Concrete heap state owner for one declaration arm. Refusal is retained
          for candidate validation and reported to the enclosing compose result. */
      class GenerationStateOwner : public BoundaryInnerStateOwner
      {
      public:
        GenerationStateOwner()
            : status_(LAZY_SCOPE_READY)
        {
        }
        virtual ~GenerationStateOwner()
        {
          this->clearOwnedStates();
        }
        LazyScopeStatus status() const
        {
          return this->status_;
        }
        virtual void noteStateAllocationFailure()
        {
          this->status_ = LAZY_SCOPE_STATES_REFUSED;
          if (this->enclosingBoundary())
            this->enclosingBoundary()->noteStateAllocationFailure();
        }
        virtual void attachEnclosingBoundary(BoundaryNode *boundary)
        {
          BoundaryInnerStateOwner::attachEnclosingBoundary(boundary);
          if (boundary)
          {
            this->setInvalidateTarget(boundary);
            this->setInvalidateCallback(&GenerationStateOwner::InvalidateThunk, this);
          }
        }
        /** Heap-only generation policy: leave the bump-only Boundary arena
            untouched so state bytes can be reclaimed at generation retirement. */
        virtual void *allocateStateMemory(size_t, size_t)
        {
          return 0;
        }

      protected:
        virtual void detachOwnedStateFromAncestors(loka::core::StateBase *state)
        {
          if (this->enclosingBoundary())
            this->enclosingBoundary()->forgetInnerOwnedState(state);
        }
        virtual void destroyOwnedStateStorage(loka::core::StateBase *state)
        {
          assert(!state->isArenaAllocated() && "generation states never live in the Boundary arena");
          DestroyAdoptedHeapState(state);
        }

      private:
        static void InvalidateThunk(void *data)
        {
          GenerationStateOwner *self = static_cast<GenerationStateOwner *>(data);
          if (self->enclosingBoundary())
            self->enclosingBoundary()->noteInnerTrackerCommit(self->tracker()->asPushTracker());
        }
        LazyScopeStatus status_;
      };

      /** Runtime root and state owner of one declaration arm. The enclosing
          Boundary owns all seat runtime rows and the retirement clock. */
      class GenerationRoot : public ComposableNode
      {
      public:
        virtual ~GenerationRoot()
        {
          this->releaseCallbacks();
          this->clearChildren();
          this->releaseNodeStateRegistrations();
        }
        virtual const void *nodeTypeKey() const
        {
          return NodeTypeToken<GenerationRoot>();
        }
        virtual IStateOwner *asStateOwner()
        {
          return &this->stateOwner_;
        }
        LazyScopeStatus scopeStatus() const
        {
          return this->stateOwner_.status();
        }
        virtual void render(IPlatformController *controller)
        {
          for (Node *child = this->childrenHead(); child; child = child->nextInComposition)
            child->render(controller);
        }
        virtual short layout(IPlatformController *controller, LayoutState &state)
        {
          short result = 0;
          for (Node *child = this->childrenHead(); child; child = child->nextInComposition)
            result = child->layout(controller, state);
          return result;
        }

      protected:
        GenerationRoot()
            : ComposableNode(),
              stateOwner_()
        {
        }
        virtual void composeWithContext(ComponentContext &context, ComposeEvent event)
        {
          if (event == COMPOSE_EVENT_DETACH)
            this->detachNode(this->beginComposition(context));
          else if (event == COMPOSE_EVENT_ATTACH)
            this->attachNode(this->beginDeclaringWindow(context));
        }

        GenerationStateOwner stateOwner_;
        friend class ::loka::dsl::testing::OwnershipDump;
      };
      /** Owns only the unpublished runtime root; materialization transfers it
          to the Boundary's existing child-first retirement clock, even when
          a partially materialized subtree reports failure. */
      class GenerationDeclaration : public BranchSeatDeclaration
      {
      public:
        GenerationDeclaration()
            : pending_()
        {
        }
        template <class NodeT, class PropsT> NodeT *createRoot(const PropsT &props, ComponentContext &context)
        {
          assert(!this->pending_.root());
          NodeDefinition<PropsT, NodeT> factory(props);
          this->composition.setContext(&context);
          NodeMaterializationResult created = this->composition.createNodeFromDefinitionResult(&factory);
          this->composition.setContext(0);
          if (context.nodeStorage())
            this->pending_.prepare(created.root, &BoundaryNode::RetireUnattachedCandidate, &context);
          else
            this->pending_.prepare(created.root);
          return created.allocationFailed ? 0 : static_cast<NodeT *>(this->pending_.root());
        }
        virtual NodeMaterializationResult materialize(ComponentContext &context, Node *)
        {
          GenerationRoot *root = static_cast<GenerationRoot *>(this->pending_.root());
          assert(root);
          ComponentContext childContext(context);
          childContext.setStateOwner(root->asStateOwner());
          childContext.setOwner(root);
          this->composition.setContext(&childContext);
          NodeMaterializationResult result = BranchSeatDeclaration::materialize(childContext, root);
          this->composition.setContext(0);
          if (result.root)
            root->addChild(result.root);
          result.root = this->pending_.take();
          return result;
        }

      private:
        PendingSubtree pending_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka
#endif
