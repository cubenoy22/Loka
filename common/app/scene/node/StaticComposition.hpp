#ifndef LOKA_CORE2_SCENE_NODE_STATICCOMPOSITION_HPP
#define LOKA_CORE2_SCENE_NODE_STATICCOMPOSITION_HPP

#include "../NodeComposition.hpp"
#include "Boundary.hpp"
#include "loka/core/Profiler.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      // Forward declaration so props can alias NodeType
      template <class PropsT>
      class StaticCompositionBoundaryNodeBase;

      struct StaticCompositionTypeTag
      {
      };

      template <class NodeT>
      struct StaticCompositionPropsForTypeTag
      {
      };

      template <class NodeT>
      struct BoundaryPropsForTypeTag
      {
      };

      template <typename T>
      struct BoundaryPropValueRules
      {
        enum
        {
          kAllowed = 1
        };
      };

      template <typename T>
      struct BoundaryPropValueRules<BoundState<T> >
      {
        enum
        {
          kAllowed = 0
        };
      };

      template <typename T>
      struct BoundaryPropValueRules<loka::core::MutableState<T> *>
      {
        enum
        {
          kAllowed = 0
        };
      };

      template <typename T>
      struct BoundaryPropValueRules<const loka::core::MutableState<T> *>
      {
        enum
        {
          kAllowed = 0
        };
      };

      template <typename T>
      inline void AssertBoundaryPropValueAllowed()
      {
        LOKA_STATIC_ASSERT(BoundaryPropValueRules<T>::kAllowed, boundary_props_must_not_hold_owned_or_raw_mutable_state);
      }

      struct StaticCompositionProps : public NodePropsBase<StaticCompositionProps>
      {
        StaticCompositionProps() {}
        typedef StaticCompositionTypeTag TypeTag;
        typedef StaticCompositionBoundaryNodeBase<StaticCompositionProps> NodeType;
        bool operator<(const PropsBase &rhs) const
        {
          if (rhs.propsTypeId() != propsTypeId())
            return false;
          return false; // no fields to compare
        }
      };

      typedef StaticCompositionBoundaryNodeBase<StaticCompositionProps> StaticCompositionBoundaryNode;
      typedef StaticCompositionBoundaryNode StaticCompositionNode;

      // Helper props for static composition boundary nodes with no custom fields.
      template <class NodeT>
      struct StaticCompositionPropsFor : public NodePropsBase<StaticCompositionPropsFor<NodeT> >
      {
        typedef StaticCompositionPropsForTypeTag<NodeT> TypeTag;
        typedef NodeT NodeType;
        bool operator<(const PropsBase &rhs) const
        {
          if (rhs.propsTypeId() != this->propsTypeId())
            return false;
          return false; // no fields to compare
        }
      };

      // Alias to avoid exposing "Static" in user code.
      // Note: Must inherit from NodePropsBase<BoundaryPropsFor<NodeT>> directly
      // so that createNode() receives the correct Props type for node constructors.
      template <class NodeT>
      struct BoundaryPropsFor : public NodePropsBase<BoundaryPropsFor<NodeT> >
      {
        typedef BoundaryPropsForTypeTag<NodeT> TypeTag;
        typedef NodeT NodeType;

        template <typename T>
        static void assertAllowedValueType()
        {
          AssertBoundaryPropValueAllowed<T>();
        }

        bool operator<(const PropsBase &rhs) const
        {
          if (rhs.propsTypeId() != this->propsTypeId())
            return false;
          return false; // no fields to compare
        }
      };

      template <class PropsT>
      class StaticCompositionBoundaryNodeBase : public BoundaryNode
      {
      public:
        typedef typename PropsT::TypeTag TypeTag;
        PropsT props;
        StaticCompositionBoundaryNodeBase(const PropsT &p)
            : BoundaryNode(), props(p), composed_(false) {}
        virtual ~StaticCompositionBoundaryNodeBase() {}

        // Build node definitions into composition container (default: no children)
        // Making this non-pure allows instantiation via NodeDefinition<StaticCompositionProps, StaticCompositionNode>
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
          this->rebuildCompositionTransactionFromSnapshots();
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

      struct StaticCompositionDefinition : public NodeDefinition<StaticCompositionProps, StaticCompositionNode>
      {
        StaticCompositionDefinition() : NodeDefinition<StaticCompositionProps, StaticCompositionNode>() {}
        StaticCompositionDefinition(const StaticCompositionProps &p) : NodeDefinition<StaticCompositionProps, StaticCompositionNode>(p) {}
      };

      struct StaticCompositionBoundaryDefinition : public BoundaryDefinition<StaticCompositionProps, StaticCompositionBoundaryNode>
      {
        StaticCompositionBoundaryDefinition() : BoundaryDefinition<StaticCompositionProps, StaticCompositionBoundaryNode>() {}
        StaticCompositionBoundaryDefinition(const StaticCompositionProps &p) : BoundaryDefinition<StaticCompositionProps, StaticCompositionBoundaryNode>(p) {}
      };

      inline StaticCompositionDefinition StaticComposition(const StaticCompositionProps &p)
      {
        return StaticCompositionDefinition(p);
      }

      inline StaticCompositionBoundaryDefinition StaticCompositionBoundary(const StaticCompositionProps &p)
      {
        return StaticCompositionBoundaryDefinition(p);
      }

      template <class NodeT>
      inline BoundaryDefinition<StaticCompositionPropsFor<NodeT>, NodeT> StaticCompositionBoundary()
      {
        return BoundaryDefinition<StaticCompositionPropsFor<NodeT>, NodeT>();
      }

      template <class NodeT>
      inline BoundaryDefinition<StaticCompositionPropsFor<NodeT>, NodeT> StaticCompositionBoundary(const StaticCompositionPropsFor<NodeT> &p)
      {
        return BoundaryDefinition<StaticCompositionPropsFor<NodeT>, NodeT>(p);
      }

      // Alias to avoid exposing "Static" in user code.
      template <class NodeT>
      inline BoundaryDefinition<BoundaryPropsFor<NodeT>, NodeT> Boundary()
      {
        return BoundaryDefinition<BoundaryPropsFor<NodeT>, NodeT>();
      }

      template <class NodeT>
      inline BoundaryDefinition<BoundaryPropsFor<NodeT>, NodeT> Boundary(const BoundaryPropsFor<NodeT> &p)
      {
        return BoundaryDefinition<BoundaryPropsFor<NodeT>, NodeT>(p);
      }

      // Helper base class for nodes using StaticCompositionPropsFor<NodeT>.
      template <class NodeT>
      class StaticCompositionNodeFor : public StaticCompositionBoundaryNodeBase<StaticCompositionPropsFor<NodeT> >
      {
      public:
        typedef StaticCompositionPropsFor<NodeT> PropsType;
        StaticCompositionNodeFor(const PropsType &p)
            : StaticCompositionBoundaryNodeBase<StaticCompositionPropsFor<NodeT> >(p) {}
        virtual ~StaticCompositionNodeFor() {}
      };

      // Alias for boundary nodes without exposing "Static" in user code.
      // Uses BoundaryPropsFor<NodeT> to match user constructor expectations.
      template <class NodeT>
      class BoundaryNodeFor : public StaticCompositionBoundaryNodeBase<BoundaryPropsFor<NodeT> >
      {
      public:
        typedef BoundaryPropsFor<NodeT> PropsType;
        BoundaryNodeFor(const PropsType &p)
            : StaticCompositionBoundaryNodeBase<BoundaryPropsFor<NodeT> >(p) {}
        virtual ~BoundaryNodeFor() {}
      };

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_NODE_STATICCOMPOSITION_HPP
