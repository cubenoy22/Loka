#ifndef LOKA_CORE2_SCENE_COMPOSITION_STDCOMPOSITION_HPP
#define LOKA_CORE2_SCENE_COMPOSITION_STDCOMPOSITION_HPP

#include "app/scene/composition/NodeComposition.hpp"
#include "app/scene/composition/impl/StdCompositionBoundaryNode.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      struct StdCompositionTypeTag
      {
      };

      template <class NodeT> struct StdCompositionPropsForTypeTag
      {
      };

      template <class NodeT> struct BoundaryPropsForTypeTag
      {
      };

      template <typename T> struct BoundaryPropValueRules
      {
        enum
        {
          kAllowed = 1
        };
      };

      template <typename T> struct BoundaryPropValueRules<NodeState<T> >
      {
        enum
        {
          kAllowed = 0
        };
      };

      template <typename T> struct BoundaryPropValueRules<loka::core::MutableState<T> *>
      {
        enum
        {
          kAllowed = 0
        };
      };

      template <typename T> struct BoundaryPropValueRules<const loka::core::MutableState<T> *>
      {
        enum
        {
          kAllowed = 0
        };
      };

      template <typename T> inline void AssertBoundaryPropValueAllowed()
      {
        LOKA_STATIC_ASSERT(BoundaryPropValueRules<T>::kAllowed,
                           boundary_props_must_not_hold_owned_or_raw_mutable_state);
      }

      struct StdCompositionProps : public NodePropsBase<StdCompositionProps>
      {
        StdCompositionProps() {}
        typedef StdCompositionTypeTag TypeTag;
        typedef StdCompositionBoundaryNodeBase<StdCompositionProps> NodeType;
        bool operator<(const PropsBase &rhs) const
        {
          if (rhs.propsTypeId() != propsTypeId())
            return false;
          return false; // no fields to compare
        }
      };

      typedef StdCompositionBoundaryNodeBase<StdCompositionProps> StdCompositionBoundaryNode;
      typedef StdCompositionBoundaryNode StdCompositionNode;

      // Helper props for standard composition boundary nodes with no custom fields.
      template <class NodeT> struct StdCompositionPropsFor : public NodePropsBase<StdCompositionPropsFor<NodeT> >
      {
        typedef StdCompositionPropsForTypeTag<NodeT> TypeTag;
        typedef NodeT NodeType;
        bool operator<(const PropsBase &rhs) const
        {
          if (rhs.propsTypeId() != this->propsTypeId())
            return false;
          return false; // no fields to compare
        }
      };

      // Alias to avoid exposing composition strategy in common user code.
      // Note: Must inherit from NodePropsBase<BoundaryPropsFor<NodeT>> directly
      // so that createNode() receives the correct Props type for node constructors.
      template <class NodeT> struct BoundaryPropsFor : public NodePropsBase<BoundaryPropsFor<NodeT> >
      {
        typedef BoundaryPropsForTypeTag<NodeT> TypeTag;
        typedef NodeT NodeType;

        template <typename T> static void assertAllowedValueType()
        {
          AssertBoundaryPropValueAllowed<T>();
        }

        template <typename T> static BorrowedState<T> borrowed(loka::core::State<T> *state)
        {
          AssertBoundaryPropValueAllowed<loka::core::State<T> *>();
          return BorrowedState<T>(state);
        }

        template <typename T> static loka::core::Managed<T> shared(const loka::core::Managed<T> &value)
        {
          AssertBoundaryPropValueAllowed<loka::core::Managed<T> >();
          return value;
        }

        bool operator<(const PropsBase &rhs) const
        {
          if (rhs.propsTypeId() != this->propsTypeId())
            return false;
          return false; // no fields to compare
        }
      };

      struct StdCompositionDefinition : public NodeDefinition<StdCompositionProps, StdCompositionNode>
      {
        StdCompositionDefinition()
            : NodeDefinition<StdCompositionProps, StdCompositionNode>()
        {
        }
        StdCompositionDefinition(const StdCompositionProps &p)
            : NodeDefinition<StdCompositionProps, StdCompositionNode>(p)
        {
        }
      };

      struct StdCompositionBoundaryDefinition
          : public BoundaryDefinition<StdCompositionProps, StdCompositionBoundaryNode>
      {
        StdCompositionBoundaryDefinition()
            : BoundaryDefinition<StdCompositionProps, StdCompositionBoundaryNode>()
        {
        }
        StdCompositionBoundaryDefinition(const StdCompositionProps &p)
            : BoundaryDefinition<StdCompositionProps, StdCompositionBoundaryNode>(p)
        {
        }
      };

      inline StdCompositionDefinition StdComposition(const StdCompositionProps &p)
      {
        return StdCompositionDefinition(p);
      }

      inline StdCompositionBoundaryDefinition StdCompositionBoundary(const StdCompositionProps &p)
      {
        return StdCompositionBoundaryDefinition(p);
      }

      template <class NodeT> inline BoundaryDefinition<StdCompositionPropsFor<NodeT>, NodeT> StdCompositionBoundary()
      {
        return BoundaryDefinition<StdCompositionPropsFor<NodeT>, NodeT>();
      }

      template <class NodeT>
      inline BoundaryDefinition<StdCompositionPropsFor<NodeT>, NodeT>
      StdCompositionBoundary(const StdCompositionPropsFor<NodeT> &p)
      {
        return BoundaryDefinition<StdCompositionPropsFor<NodeT>, NodeT>(p);
      }

      // Alias to avoid exposing composition strategy in common user code.
      template <class NodeT> inline BoundaryDefinition<BoundaryPropsFor<NodeT>, NodeT> Boundary()
      {
        return BoundaryDefinition<BoundaryPropsFor<NodeT>, NodeT>();
      }

      template <class NodeT>
      inline BoundaryDefinition<BoundaryPropsFor<NodeT>, NodeT> Boundary(const BoundaryPropsFor<NodeT> &p)
      {
        return BoundaryDefinition<BoundaryPropsFor<NodeT>, NodeT>(p);
      }

      // Helper base class for nodes using StdCompositionPropsFor<NodeT>.
      template <class NodeT>
      class StdCompositionNodeFor : public StdCompositionBoundaryNodeBase<StdCompositionPropsFor<NodeT> >
      {
      public:
        typedef StdCompositionPropsFor<NodeT> PropsType;
        StdCompositionNodeFor(const PropsType &p)
            : StdCompositionBoundaryNodeBase<StdCompositionPropsFor<NodeT> >(p)
        {
        }
        virtual ~StdCompositionNodeFor() {}
      };

      // Alias for boundary nodes without exposing composition strategy in user code.
      // Uses BoundaryPropsFor<NodeT> to match user constructor expectations.
      template <class NodeT> class BoundaryNodeFor : public StdCompositionBoundaryNodeBase<BoundaryPropsFor<NodeT> >
      {
      public:
        typedef BoundaryPropsFor<NodeT> PropsType;
        BoundaryNodeFor(const PropsType &p)
            : StdCompositionBoundaryNodeBase<BoundaryPropsFor<NodeT> >(p)
        {
        }
        virtual ~BoundaryNodeFor() {}
      };

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_COMPOSITION_STDCOMPOSITION_HPP
