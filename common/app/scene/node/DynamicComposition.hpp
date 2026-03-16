#ifndef LOKA_CORE2_SCENE_NODE_DYNAMICCOMPOSITION_HPP
#define LOKA_CORE2_SCENE_NODE_DYNAMICCOMPOSITION_HPP

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
      class DynamicCompositionBoundaryNodeBase;

      struct DynamicCompositionTypeTag
      {
      };

      template <class NodeT>
      struct DynamicCompositionPropsForTypeTag
      {
      };

      struct DynamicCompositionProps : public NodePropsBase<DynamicCompositionProps>
      {
        DynamicCompositionProps() {}
        typedef DynamicCompositionTypeTag TypeTag;
        typedef DynamicCompositionBoundaryNodeBase<DynamicCompositionProps> NodeType;
        bool operator<(const PropsBase &rhs) const
        {
          if (rhs.propsTypeId() != propsTypeId())
            return false;
          return false; // no fields to compare
        }
      };

      typedef DynamicCompositionBoundaryNodeBase<DynamicCompositionProps> DynamicCompositionBoundaryNode;
      typedef DynamicCompositionBoundaryNode DynamicCompositionNode;

      // Helper props for dynamic composition boundary nodes with no custom fields.
      template <class NodeT>
      struct DynamicCompositionPropsFor : public NodePropsBase<DynamicCompositionPropsFor<NodeT> >
      {
        typedef DynamicCompositionPropsForTypeTag<NodeT> TypeTag;
        typedef NodeT NodeType;
        bool operator<(const PropsBase &rhs) const
        {
          if (rhs.propsTypeId() != this->propsTypeId())
            return false;
          return false; // no fields to compare
        }
      };

      template <class PropsT>
      class DynamicCompositionBoundaryNodeBase : public BoundaryNode
      {
      public:
        typedef typename PropsT::TypeTag TypeTag;
        PropsT props;
        DynamicCompositionBoundaryNodeBase(const PropsT &p)
            : BoundaryNode(), props(p) {}
        virtual ~DynamicCompositionBoundaryNodeBase() {}
        virtual bool flushViewDirtyImmediately(NodeDirtyFlags flags) const
        {
          (void)flags;
          return false;
        }

        // Build node definitions into composition container (default: no children)
        virtual void composeNode(NodeComposition &c) { (void)c; }

        void detachExistingChildren(ComponentContext &context)
        {
          loka::dsl::CompositionCursor<Node> it(this->childrenHead(), this->childrenCount());
          for (Node *child = it.next(); child; child = it.next())
          {
            this->composeTree(child, context, COMPOSE_EVENT_DETACH, this);
          }
        }

        virtual void composeWithContext(ComponentContext &context, ComposeEvent event)
        {
          PROFILE_FUNC();
          if (event == COMPOSE_EVENT_DETACH)
          {
            this->detachExistingChildren(context);
            NodeComposition &composition = this->beginComposition(context);
            this->detachNode(composition);
            return;
          }
          if (event != COMPOSE_EVENT_ATTACH && event != COMPOSE_EVENT_UPDATE)
          {
            return;
          }
          // UPDATE時はdirtyフラグをチェック
          // NODE_DIRTY_CHILDがなければ子構造は変わらないのでrecomposeをスキップ
          if (event == COMPOSE_EVENT_UPDATE)
          {
            NodeDirtyFlags flags = context.dirtyFlags();
            if (!(flags & NODE_DIRTY_CHILD))
            {
              // 子構造は変わらない、既存ノードを維持
              // 子ノードにはUPDATEイベントを伝播
              loka::dsl::CompositionCursor<Node> it(this->childrenHead(), this->childrenCount());
              for (Node *child = it.next(); child; child = it.next())
              {
                this->composeTree(child, context, event, this);
              }
              return;
            }
          }
          NodeComposition &composition = this->beginComposition(context);
          if (event == COMPOSE_EVENT_ATTACH)
          {
            this->attachNode(composition);
          }
          {
            NodeComposition::CompositionScope scope(composition);
            this->composeNode(composition);
          }
          this->captureCurrentCompositionSnapshot();
          this->rebuildCompositionTransactionFromSnapshots();
          if (event == COMPOSE_EVENT_UPDATE && this->canApplyLocalCompositionDiff() && this->localCompositionDiff()->isCompatibleRetainOnly())
          {
            if (!this->localCompositionDiff()->isStableRetainOnly())
            {
              for (NodeCompositionDiff::Entry *entry = this->localCompositionDiff()->entriesHead(); entry; entry = entry->nextInComposition)
              {
                if (!entry->equivalentProps)
                {
                  this->applyCurrentDefinitionPropsToLiveChild(entry->tag);
                }
              }
            }
            this->promoteCurrentCompositionSnapshot();
            loka::dsl::CompositionCursor<Node> it(this->childrenHead(), this->childrenCount());
            for (Node *child = it.next(); child; child = it.next())
            {
              this->composeTree(child, context, event, this);
            }
            return;
          }
          if (event == COMPOSE_EVENT_UPDATE && this->canApplyLocalCompositionDiff() && this->localCompositionDiff()->isCompatibleRetainOrReplaceOnly())
          {
            bool localApplied = true;
            for (NodeCompositionDiff::Entry *entry = this->localCompositionDiff()->entriesHead(); entry; entry = entry->nextInComposition)
            {
              if (entry->action == NodeCompositionDiff::ACTION_RETAIN)
              {
                if (!entry->equivalentProps && !this->applyCurrentDefinitionPropsToLiveChild(entry->tag))
                {
                  localApplied = false;
                  break;
                }
                continue;
              }
              if (entry->action == NodeCompositionDiff::ACTION_REPLACE)
              {
                if (!this->replaceCompositionChildFromCurrentDefinition(entry->tag, context))
                {
                  localApplied = false;
                  break;
                }
              }
            }
            if (localApplied)
            {
              this->promoteCurrentCompositionSnapshot();
              for (NodeCompositionDiff::Entry *entry = this->localCompositionDiff()->entriesHead(); entry; entry = entry->nextInComposition)
              {
                if (entry->action != NodeCompositionDiff::ACTION_RETAIN)
                {
                  continue;
                }
                Node *child = this->findCompositionChildByTag(entry->tag);
                if (child)
                {
                  this->composeTree(child, context, event, this);
                }
              }
              return;
            }
          }
          NodeComposition detachComposition;
          detachComposition.setContext(&context);
          this->detachNode(detachComposition);
          this->detachExistingChildren(context);
          this->clearChildren();
          // Reset arena for this boundary compose pass.
          this->nodeArena()->clear();
          this->promoteCurrentCompositionSnapshot();
          context.setComposition(&composition);
          Node *child = composition.createNodeTree();
          if (child)
          {
            this->addChild(child);
            this->composeTree(child, context, event, this);
          }
          context.setComposition(0);
        }
      };

      struct DynamicCompositionDefinition : public NodeDefinition<DynamicCompositionProps, DynamicCompositionNode>
      {
        DynamicCompositionDefinition() : NodeDefinition<DynamicCompositionProps, DynamicCompositionNode>() {}
        DynamicCompositionDefinition(const DynamicCompositionProps &p) : NodeDefinition<DynamicCompositionProps, DynamicCompositionNode>(p) {}
      };

      struct DynamicCompositionBoundaryDefinition : public BoundaryDefinition<DynamicCompositionProps, DynamicCompositionBoundaryNode>
      {
        DynamicCompositionBoundaryDefinition() : BoundaryDefinition<DynamicCompositionProps, DynamicCompositionBoundaryNode>() {}
        DynamicCompositionBoundaryDefinition(const DynamicCompositionProps &p) : BoundaryDefinition<DynamicCompositionProps, DynamicCompositionBoundaryNode>(p) {}
      };

      inline DynamicCompositionDefinition DynamicComposition(const DynamicCompositionProps &p)
      {
        return DynamicCompositionDefinition(p);
      }

      inline DynamicCompositionBoundaryDefinition DynamicCompositionBoundary(const DynamicCompositionProps &p)
      {
        return DynamicCompositionBoundaryDefinition(p);
      }

      template <class NodeT>
      inline BoundaryDefinition<DynamicCompositionPropsFor<NodeT>, NodeT> DynamicCompositionBoundary()
      {
        return BoundaryDefinition<DynamicCompositionPropsFor<NodeT>, NodeT>();
      }

      template <class NodeT>
      inline BoundaryDefinition<DynamicCompositionPropsFor<NodeT>, NodeT> DynamicCompositionBoundary(const DynamicCompositionPropsFor<NodeT> &p)
      {
        return BoundaryDefinition<DynamicCompositionPropsFor<NodeT>, NodeT>(p);
      }

      // Helper base class for nodes using DynamicCompositionPropsFor<NodeT>.
      template <class NodeT>
      class DynamicCompositionNodeFor : public DynamicCompositionBoundaryNodeBase<DynamicCompositionPropsFor<NodeT> >
      {
      public:
        typedef DynamicCompositionPropsFor<NodeT> PropsType;
        DynamicCompositionNodeFor(const PropsType &p)
            : DynamicCompositionBoundaryNodeBase<DynamicCompositionPropsFor<NodeT> >(p) {}
        virtual ~DynamicCompositionNodeFor() {}
      };

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_NODE_DYNAMICCOMPOSITION_HPP
