#ifndef LOKA_CORE2_SCENE_COMPOSITION_IMPL_STDCOMPOSITIONBOUNDARYNODE_HPP
#define LOKA_CORE2_SCENE_COMPOSITION_IMPL_STDCOMPOSITIONBOUNDARYNODE_HPP

#include "app/scene/boundary/Boundary.hpp"
#include "app/scene/composition/NodeComposition.hpp"
#include "core/Profiler.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      template <class PropsT> class StdCompositionBoundaryNodeBase : public BoundaryNode
      {
      public:
        typedef PropsT PropsType;
        typedef typename PropsT::TypeTag TypeTag;
        PropsT props;
        StdCompositionBoundaryNodeBase(const PropsT &p)
            : BoundaryNode(),
              props(p),
              composed_(false)
        {
        }
        virtual ~StdCompositionBoundaryNodeBase() {}

        /** ATTACH builds or replays children; UPDATE evaluates seats and walks them. */
        virtual bool ownsChildTraversal(ComposeEvent event) const
        {
          return event == COMPOSE_EVENT_ATTACH || event == COMPOSE_EVENT_UPDATE;
        }

        // Build node definitions into composition container (default: no children)
        // Making this non-pure allows instantiation via NodeDefinition<StdCompositionProps, StdCompositionNode>
        virtual void composeNode(NodeComposition &c)
        {
          (void)c;
        }

        virtual void composeWithContext(ComponentContext &context, ComposeEvent event)
        {
          if (event == COMPOSE_EVENT_DETACH)
          {
            NodeComposition &composition = this->beginComposition(context);
            this->detachNode(composition);
            this->composed_ = false;
            return;
          }
          if (event == COMPOSE_EVENT_UPDATE)
          {
            if (this->isFrozen())
            {
              return;
            }
            if (this->composed_)
            {
              this->updateCompositionChildren(context);
              return;
            }
            // No admitted tree: recovery is ATTACH, never an UPDATE declaration.
            event = COMPOSE_EVENT_ATTACH;
          }
          if (event != COMPOSE_EVENT_ATTACH)
          {
            return;
          }
          if (this->composed_)
          {
            this->composeOwnedChildren(context, event);
            return;
          }
          if (!this->composition().root())
          {
            if (this->childrenHead())
            {
              this->clearChildren();
              this->nodeArena()->clear();
            }
            NodeComposition &composition = this->beginDeclaringWindow(context);
            {
              PROFILE_SECTION("attach");
              this->attachNode(composition);
            }
            {
              PROFILE_SECTION("compNode");
              NodeComposition::CompositionScope scope(composition);
              this->composeNode(composition);
            }
            if (!this->finishInitialDeclaration())
              return;
          }
          this->composed_ = this->materializeInitialChildren(context);
        }

      private:
        bool composed_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_COMPOSITION_IMPL_STDCOMPOSITIONBOUNDARYNODE_HPP
