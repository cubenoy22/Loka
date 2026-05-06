#ifndef LOKA_CORE2_SCENE_COMPOSITION_IMPL_STDCOMPOSITIONBOUNDARYNODE_HPP
#define LOKA_CORE2_SCENE_COMPOSITION_IMPL_STDCOMPOSITIONBOUNDARYNODE_HPP

#include "app/scene/boundary/Boundary.hpp"
#include "app/scene/composition/NodeComposition.hpp"
#include "loka/core/Profiler.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      template <class PropsT>
      class StdCompositionBoundaryNodeBase : public BoundaryNode
      {
      public:
        typedef typename PropsT::TypeTag TypeTag;
        PropsT props;
        StdCompositionBoundaryNodeBase(const PropsT &p)
            : BoundaryNode(), props(p), composed_(false) {}
        virtual ~StdCompositionBoundaryNodeBase() {}

        // Build node definitions into composition container (default: no children)
        // Making this non-pure allows instantiation via NodeDefinition<StdCompositionProps, StdCompositionNode>
        virtual void composeNode(NodeComposition &c) {}

        virtual void composeWithContext(ComponentContext &context, ComposeEvent event)
        {
          if (event == COMPOSE_EVENT_DETACH)
          {
            NodeComposition &composition = this->beginComposition(context);
            this->detachNode(composition);
            composed_ = false;
            return;
          }
          if (event == COMPOSE_EVENT_UPDATE)
          {
            if (this->isFrozen())
            {
              return;
            }
            if (!composed_)
            {
              return;
            }
            loka::dsl::CompositionCursor<Node> it(this->childrenHead(), this->childrenCount());
            for (Node *child = it.next(); child; child = it.next())
            {
              this->composeTree(child, context, event, this);
            }
            return;
          }
          if (event != COMPOSE_EVENT_ATTACH)
          {
            return;
          }
          if (composed_)
          {
            return;
          }
          this->clearChildren();
          this->nodeArena()->clear();
          NodeComposition &composition = this->beginComposition(context);
          {
            PROFILE_SECTION("attach");
            this->attachNode(composition);
          }
          {
            PROFILE_SECTION("compNode");
            NodeComposition::CompositionScope scope(composition);
            this->composeNode(composition);
          }
          this->captureCurrentCompositionSnapshot();
          this->rebuildCurrentCompositionDiff();
          this->promoteCurrentCompositionSnapshot();
          // Pass composition to children via context
          context.setComposition(&composition);
          Node *child;
          {
            PROFILE_SECTION("create");
            child = composition.createNodeTree();
          }
          if (child)
          {
            this->addChild(child);
            this->composeTree(child, context, event, this);
          }
          context.setComposition(0);
          composed_ = true;
        }

      private:
        bool composed_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_COMPOSITION_IMPL_STDCOMPOSITIONBOUNDARYNODE_HPP
