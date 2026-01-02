#ifndef DECLARA_CORE2_SCENE_NODE_COMPOSABLENODE_HPP
#define DECLARA_CORE2_SCENE_NODE_COMPOSABLENODE_HPP

#include <cassert>
#include <vector>
#include "../Node.hpp"
#include "../ComponentContext.hpp"
#include "../NodeComposition.hpp"
#include "../StateOwner.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {
      enum ContextPlacement
      {
        CONTEXT_PLACEMENT_BOUNDARY = 0,
        CONTEXT_PLACEMENT_ROOT = 1
      };

      // Base class for nodes that build children via compose() when requested
      class ComposableNode : public NestableNode
      {
      public:
        ComposableNode() : currentContext_(0) {}
        virtual ~ComposableNode()
        {
          clearChildren();
        }

        virtual void compose(ComponentContext &context, ComposeEvent event)
        {
          ContextScope scope(this, &context);
          this->composeWithContext(context, event);
        }

        virtual void compose(ComponentContext &context)
        {
          this->compose(context, COMPOSE_EVENT_ATTACH);
        }

      protected:
        virtual void composeWithContext(ComponentContext &context, ComposeEvent event) = 0;

        ComponentContext *componentContext() const { return currentContext_; }

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
              node->currentContext_->setStateOwner(dynamic_cast<IStateOwner *>(n));
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
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_NODE_COMPOSABLENODE_HPP
