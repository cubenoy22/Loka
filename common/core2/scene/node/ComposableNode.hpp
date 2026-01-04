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
        ComposableNode() : currentContext_(0), isAttached_(false), attached_() {}
        virtual ~ComposableNode()
        {
          clearChildren();
        }

        virtual void compose(ComponentContext &context, ComposeEvent event)
        {
          ContextScope scope(this, &context);
          attached_.boundary_ = context.boundary();
          attached_.scene_ = context.scene();
          attached_.window_ = context.window();
          isAttached_ = attached_.boundary_ && attached_.scene_ && attached_.window_;
          this->composeWithContext(context, event);
          if (event == COMPOSE_EVENT_DETACH)
          {
            attached_.reset();
            isAttached_ = false;
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
        struct AttachedContext
        {
          AttachedContext() : boundary_(0), scene_(0), window_(0) {}
          BoundaryNode *boundary() const { return boundary_; }
          Scene *scene() const { return scene_; }
          ::Window *window() const { return window_; }
          void reset()
          {
            boundary_ = 0;
            scene_ = 0;
            window_ = 0;
          }

          BoundaryNode *boundary_;
          Scene *scene_;
          ::Window *window_;
        };

        const AttachedContext *attachedContext() const
        {
          return isAttached_ ? &attached_ : 0;
        }

        const AttachedContext &ensureAttached() const
        {
          assert(isAttached_ && "ComposableNode requires attached context");
          return attached_;
        }

        BoundaryNode *boundary() const
        {
          return ensureAttached().boundary();
        }
        Scene *scene() const
        {
          return ensureAttached().scene();
        }
        ::Window *window() const
        {
          return ensureAttached().window();
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
        bool isAttached_;
        AttachedContext attached_;
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // LOKA_CORE2_SCENE_NODE_COMPOSABLENODE_HPP
