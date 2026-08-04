#ifndef LOKA_CORE2_SCENE_NODE_COMPONENT_NODE_HPP
#define LOKA_CORE2_SCENE_NODE_COMPONENT_NODE_HPP

#include "app/scene/composition/NodeComposition.hpp"
#include "app/scene/node/ComposableNode.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      /** Nested component box: owner-scope residents plus a fixed child
          subtree, materialized exactly once per structural lifetime.

          composeChildren() runs strictly after this node's queued state
          registrations connect to the nearest enclosing owner scope
          (ComposableNode::compose connects before composeWithContext), so
          child definitions may carry pointers to this node's residents.

          The subtree's structure is immutable between materialization and
          retirement; every dynamic flows through states. A structural
          change is an identity change: give the enclosing seat (for
          example a Section value key) a new value and the boundary plan
          retires this box wholesale, residents included. The plan never
          reconciles into composables (reconcileParkedBranch returns at
          asComposable()), which is what makes the subtree built here
          exclusively this node's. Branch seats (Show / Conditional)
          materialize only through Boundary plan application and therefore
          cannot appear under composeChildren(). */
      class ComponentNode : public ComposableNode
      {
      public:
        ComponentNode()
            : ComposableNode()
        {
        }

        virtual void render(IPlatformController *controller)
        {
          Node *child = this->childrenHead();
          while (child)
          {
            child->render(controller);
            child = child->nextInComposition;
          }
        }

        virtual short layout(IPlatformController *controller,
                             LayoutState &state)
        {
          short result = 0;
          Node *child = this->childrenHead();
          while (child)
          {
            result = child->layout(controller, state);
            child = child->nextInComposition;
          }
          return result;
        }

      protected:
        /** The component's single door: declare the fixed child subtree.
            Residents are already connected when this runs. */
        virtual void composeChildren(NodeComposition &composition) = 0;

        virtual void composeWithContext(ComponentContext &context,
                                        ComposeEvent event)
        {
          if (event == COMPOSE_EVENT_DETACH)
          {
            NodeComposition &composition = this->beginComposition(context);
            this->detachNode(composition);
            return;
          }
          if (event != COMPOSE_EVENT_ATTACH)
          {
            return;
          }
          NodeComposition &composition = this->beginComposition(context);
          this->attachNode(composition);
          // A parked re-entry re-attaches with the structure alive; the
          // childrenHead guard, not a flag, is what keeps re-attach from
          // materializing a second subtree next to the retained one.
          if (this->childrenHead())
          {
            return;
          }
          {
            NodeComposition::CompositionScope scope(composition);
            this->composeChildren(composition);
          }
          NodeMaterializationResult result =
              composition.createNodeTreeCompleted();
          if (result.allocationFailed || result.requiresBoundaryPlan ||
              !result.root)
          {
            // Failure atomicity: a partial subtree must never become
            // childrenHead(), or the re-attach guard would publish it
            // permanently. Heap-provenance roots return through their own
            // door; arena candidates stay ledger-owned and are reclaimed by
            // the generation retire (#150's plan-side guard is the
            // precedent). requiresBoundaryPlan is a branch seat declared
            // under a component -- seats materialize only through Boundary
            // plan application, so the whole box refuses.
            if (result.root && result.root->arenaOwner() == 0)
            {
              DestroyHeapNode(result.root);
            }
            return;
          }
          this->addChild(result.root);
        }
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_NODE_COMPONENT_NODE_HPP
