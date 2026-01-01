#ifndef DECLARA_CORE2_SCENE_NODE_COMPOSABLENODE_HPP
#define DECLARA_CORE2_SCENE_NODE_COMPOSABLENODE_HPP

#include <cassert>
#include <vector>
#include "../Node.hpp"
#include "../ComponentContext.hpp"
#include "../ContextDefinition.hpp"
#include "../NodeComposition.hpp"

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
          clearOwnedContexts();
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

        template <class T>
        T &useContext(const ContextDefinition<T> &definition, ContextPlacement placement = CONTEXT_PLACEMENT_BOUNDARY)
        {
          ComponentContext *ctx = this->componentContext();
          if (!ctx)
          {
            return ensureLocalContext<T>(definition);
          }
          if (placement == CONTEXT_PLACEMENT_ROOT)
          {
            ComponentContext *target = ctx;
            while (target->parent())
            {
              target = target->parent();
            }
            ComposableNode *owner = target->owner();
            if (owner)
            {
              return owner->ensureLocalContext<T>(definition);
            }
          }
          return ensureLocalContext<T>(definition);
        }

        template <class T>
        void provideContext(const ContextDefinition<T> &definition, T &value)
        {
          if (!currentContext_)
          {
            return;
          }
          currentContext_->provide(definition, value);
        }

        template <class T>
        T &requireContext(const ContextDefinition<T> &definition) const
        {
          assert(currentContext_ && "requireContext: ComponentContext unavailable");
          return currentContext_->require(definition);
        }

        void clearChildren()
        {
          for (size_t i = 0; i < children_.size(); ++i)
          {
            delete children_[i];
          }
          children_.clear();
        }

      private:
        struct OwnedContext
        {
          int definitionId;
          void *value;
          void (*deleter)(void *);
        };

        template <class T>
        OwnedContext *createOwnedContext(int definitionId)
        {
          T *instance = new T();
          OwnedContext owned;
          owned.definitionId = definitionId;
          owned.value = instance;
          owned.deleter = &deleteContext<T>;
          ownedContexts_.push_back(owned);
          return &ownedContexts_.back();
        }

        OwnedContext *findOwnedContext(int definitionId)
        {
          for (size_t i = 0; i < ownedContexts_.size(); ++i)
          {
            if (ownedContexts_[i].definitionId == definitionId)
            {
              return &ownedContexts_[i];
            }
          }
          return 0;
        }

        void clearOwnedContexts()
        {
          for (size_t i = 0; i < ownedContexts_.size(); ++i)
          {
            if (ownedContexts_[i].deleter && ownedContexts_[i].value)
            {
              ownedContexts_[i].deleter(ownedContexts_[i].value);
            }
          }
          ownedContexts_.clear();
        }

        template <class T>
        static void deleteContext(void *value)
        {
          delete static_cast<T *>(value);
        }

        template <class T>
        T &ensureLocalContext(const ContextDefinition<T> &definition)
        {
          ComponentContext *ctx = this->componentContext();
          if (ctx)
          {
            T *found = ctx->find(definition);
            if (found)
            {
              return *found;
            }
          }
          OwnedContext *owned = findOwnedContext(definition.id());
          if (!owned)
          {
            owned = createOwnedContext<T>(definition.id());
          }
          T *value = static_cast<T *>(owned->value);
          if (ctx)
          {
            ctx->provide(definition, *value);
          }
          return *value;
        }

        struct ContextScope
        {
          ComposableNode *node;
          ComponentContext *previous;
          ComposableNode *previousOwner;
          ContextScope(ComposableNode *n, ComponentContext *ctx) : node(n), previous(n->currentContext_), previousOwner(0)
          {
            node->currentContext_ = ctx;
            if (node->currentContext_)
            {
              previousOwner = node->currentContext_->owner();
              node->currentContext_->setOwner(n);
            }
          }
          ~ContextScope()
          {
            node->currentContext_ = previous;
            if (previous && previousOwner)
            {
              previous->setOwner(previousOwner);
            }
          }
        };

        ComponentContext *currentContext_;
        std::vector<OwnedContext> ownedContexts_;
        NodeComposition composition_;
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_NODE_COMPOSABLENODE_HPP
