#ifndef LOKA_CORE2_SCENE_NODE_COMPOSABLENODE_HPP
#define LOKA_CORE2_SCENE_NODE_COMPOSABLENODE_HPP

#include <cassert>
#include <vector>
#include "../Node.hpp"
#include "../ComponentContext.hpp"
#include "../NodeComposition.hpp"
#include "../StateOwner.hpp"

class Window;

namespace declara
{
  namespace core
  {
    namespace scene
    {
      class BoundaryNode;
      class Scene;

      enum ContextPlacement
      {
        CONTEXT_PLACEMENT_BOUNDARY = 0,
        CONTEXT_PLACEMENT_ROOT = 1
      };

      // Base class for nodes that build children via compose() when requested
      class ComposableNode : public NestableNode
      {
      public:
        ComposableNode() : currentContext_(0), cachedBoundary_(0), cachedScene_(0), cachedWindow_(0) {}
        virtual ~ComposableNode()
        {
          clearChildren();
        }

        virtual void compose(ComponentContext &context, ComposeEvent event)
        {
          ContextScope scope(this, &context);
          cachedBoundary_ = context.boundary();
          cachedScene_ = context.scene();
          cachedWindow_ = context.window();
          this->composeWithContext(context, event);
          if (event == COMPOSE_EVENT_DETACH)
          {
            cachedBoundary_ = 0;
            cachedScene_ = 0;
            cachedWindow_ = 0;
          }
        }

        virtual void compose(ComponentContext &context)
        {
          this->compose(context, COMPOSE_EVENT_ATTACH);
        }

      protected:
        virtual void composeWithContext(ComponentContext &context, ComposeEvent event) = 0;
        virtual void attachNode(NodeComposition &c) { (void)c; }
        virtual void detachNode(NodeComposition &c) { (void)c; }

        ComponentContext *componentContext() const { return currentContext_; }
        BoundaryNode *boundary() const
        {
          assert(cachedBoundary_ && "ComposableNode::boundary requires BoundaryNode");
          return cachedBoundary_;
        }
        Scene *scene() const
        {
          assert(cachedScene_ && "ComposableNode::scene requires Scene");
          return cachedScene_;
        }
        ::Window *window() const
        {
          assert(cachedWindow_ && "ComposableNode::window requires Window");
          return cachedWindow_;
        }

        NodeComposition &beginComposition(ComponentContext &context)
        {
          composition_.reset();
          composition_.setContext(&context);
          return composition_;
        }

        NodeComposition &composition() { return composition_; }
        const NodeComposition &composition() const { return composition_; }

        void clearChildren()
        {
          for (size_t i = 0; i < children_.size(); ++i)
          {
            delete children_[i];
          }
          children_.clear();
        }

      private:
        struct ContextScope
        {
          ComposableNode *node;
          ComponentContext *previous;
          Node *previousOwner;
          IStateOwner *previousStateOwner;
          ContextScope(ComposableNode *n, ComponentContext *ctx)
              : node(n), previous(n->currentContext_), previousOwner(0), previousStateOwner(0)
          {
            node->currentContext_ = ctx;
            if (node->currentContext_)
            {
              previousOwner = node->currentContext_->owner();
              node->currentContext_->setOwner(n);
              previousStateOwner = node->currentContext_->stateOwner();
              IStateOwner *currentOwner = dynamic_cast<IStateOwner *>(n);
              node->currentContext_->setStateOwner(currentOwner ? currentOwner : previousStateOwner);
            }
          }
          ~ContextScope()
          {
            node->currentContext_ = previous;
            if (previous && previousOwner)
            {
              previous->setOwner(previousOwner);
            }
            if (previous)
            {
              previous->setStateOwner(previousStateOwner);
            }
          }
        };

        ComponentContext *currentContext_;
        NodeComposition composition_;
        BoundaryNode *cachedBoundary_;
        Scene *cachedScene_;
        ::Window *cachedWindow_;
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // LOKA_CORE2_SCENE_NODE_COMPOSABLENODE_HPP
