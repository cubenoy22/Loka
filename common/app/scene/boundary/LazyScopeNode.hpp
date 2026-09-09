#ifndef LOKA_LAZY_SCOPE_NODE_HPP
#define LOKA_LAZY_SCOPE_NODE_HPP

#include "app/scene/boundary/Boundary.hpp"
#include "app/scene/boundary/BoundaryInnerStateOwner.hpp"
#include "app/scene/node/ComposableNode.hpp"

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

      /** Concrete owner for one declaration arm. Storage and invalidation follow
          BoundarySection; refusal belongs to the unpublished arm itself. */
      class LazyScopeStateOwner : public BoundaryInnerStateOwner
      {
      public:
        LazyScopeStateOwner()
            : status_(LAZY_SCOPE_READY)
        {
        }
        virtual ~LazyScopeStateOwner()
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
        }
        virtual void attachEnclosingBoundary(BoundaryNode *boundary)
        {
          BoundaryInnerStateOwner::attachEnclosingBoundary(boundary);
          if (boundary)
          {
            this->setInvalidateTarget(boundary);
            this->setInvalidateCallback(&LazyScopeStateOwner::InvalidateThunk, this);
          }
        }
        // Keep the arena/ancestor protocol paired with BoundarySectionNode.
        virtual void reserveStateArena(size_t bytes)
        {
          if (this->enclosingBoundary())
            this->enclosingBoundary()->reserveStateArena(bytes);
        }
        virtual void *allocateStateMemory(size_t bytes, size_t align)
        {
          return this->enclosingBoundary() ? this->enclosingBoundary()->allocateStateMemory(bytes, align) : 0;
        }
        virtual void registerStateMemory(loka::core::StateBase *state, void (*destroy)(loka::core::StateBase *))
        {
          assert(this->enclosingBoundary());
          this->enclosingBoundary()->registerStateMemory(state, destroy);
        }

      protected:
        virtual void detachOwnedStateFromAncestors(loka::core::StateBase *state)
        {
          if (this->enclosingBoundary())
            this->enclosingBoundary()->forgetInnerOwnedState(state);
        }
        virtual void destroyOwnedStateStorage(loka::core::StateBase *state)
        {
          if (state->isArenaAllocated())
            this->enclosingBoundary()->releaseInnerArenaStateMemory(state);
          else
            DestroyAdoptedHeapState(state);
        }

      private:
        static void InvalidateThunk(void *data)
        {
          LazyScopeStateOwner *self = static_cast<LazyScopeStateOwner *>(data);
          if (self->enclosingBoundary())
            self->enclosingBoundary()->noteInnerTrackerCommit(self->tracker()->asPushTracker());
        }
        LazyScopeStatus status_;
      };

      template <class K, class NodeT> class LazyScopeDefinition;

      /** Runtime root and state owner of a keyed declaration arm. Constructor
          state declarations connect in the candidate window before declareScope.
          The enclosing Boundary owns all seat runtime rows and the retire clock. */
      class LazyScopeNode : public ComposableNode
      {
      public:
        virtual ~LazyScopeNode()
        {
          this->releaseCallbacks();
          this->clearChildren();
          this->releaseNodeStateRegistrations();
        }
        virtual const void *nodeTypeKey() const
        {
          return NodeTypeToken<LazyScopeNode>();
        }
        virtual IStateOwner *asStateOwner()
        {
          return &this->stateOwner_;
        }
        LazyScopeStatus scopeStatus() const
        {
          return this->stateOwner_.status();
        }
        virtual void declareScope(NodeComposition &composition) = 0;
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
        LazyScopeNode()
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

      private:
        friend class ::loka::dsl::testing::OwnershipDump;
        template <class K, class NodeT> friend class LazyScopeDefinition;
        void prepareScope(ComponentContext &context, NodeComposition &composition)
        {
          this->stateOwner_.attachEnclosingBoundary(context.boundary());
          this->stateOwner_.attachEnclosingHoldOwner(context.stateOwner());
          ContextScope scope(this, &context);
          this->nodeStateOwner_ = &this->stateOwner_;
          this->connectNodeStateRegistrations();
          if (this->scopeStatus() != LAZY_SCOPE_READY)
            return;
          this->beginDeclaringWindow(context);
          if (this->scopeStatus() != LAZY_SCOPE_READY)
            return;
          assert(context.owner() == this && context.stateOwner() == &this->stateOwner_);
          composition.setContext(&context);
          {
            NodeComposition::CompositionScope window(composition);
            this->declareScope(composition);
          }
          composition.setContext(0);
        }
        LazyScopeStateOwner stateOwner_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka
#endif
