#ifndef LOKA_CORE2_SCENE_NODECOMPOSITION_HPP
#define LOKA_CORE2_SCENE_NODECOMPOSITION_HPP

#include <vector>
#include <cassert>
#include "core2/scene/Node.hpp"
#include "core2/scene/StreamView.hpp"
#include "core2/scene/node/Conditional.hpp"
#include "core2/scene/BoundState.hpp"
#include "core2/scene/ComponentContext.hpp"
#include "core2/scene/StateOwner.hpp"

class Window;

namespace declara
{
  namespace core
  {
    namespace scene
    {
      class BoundaryNode;
      class Scene;

      struct NodeComposition
      {
      public:
        class StateBatch
        {
        public:
          explicit StateBatch(NodeComposition *composition);
          StateBatch(const StateBatch &other);
          StateBatch &operator=(const StateBatch &other);
          ~StateBatch();

          template <typename T>
          StateBatch &state(BoundState<T> &out, const T &initial)
          {
            if (!impl_ || impl_->done)
            {
              return *this;
            }
            impl_->specs.push_back(new Spec<T>(out, initial));
            ++impl_->count;
            return *this;
          }

        private:
          struct SpecBase
          {
            virtual ~SpecBase() {}
            virtual void apply(NodeComposition *composition) = 0;
          };

          template <typename T>
          struct Spec : public SpecBase
          {
            Spec(BoundState<T> &outRef, const T &value) : out(&outRef), initial(value) {}
            virtual void apply(NodeComposition *composition)
            {
              assert(composition && "StateBatch::apply requires composition");
              IStateOwner *stateOwner = composition->context_->stateOwner();
              assert(stateOwner && "StateBatch::apply requires Boundary owner");
              MutableState<T> *state = new MutableState<T>(initial);
              stateOwner->adoptStateUnchecked(state);
              *out = BoundState<T>(state, stateOwner->tracker());
            }
            BoundState<T> *out;
            T initial;
          };

          struct Impl
          {
            explicit Impl(NodeComposition *composition)
                : composition(composition), count(0), refs(1), done(false), specs() {}
            NodeComposition *composition;
            size_t count;
            int refs;
            bool done;
            std::vector<SpecBase *> specs;
          };

          Impl *impl_;
          void finalize();
          void release();
        };

      private:
        // Arena: owns copies of all definitions created during compose
        std::vector<NodeDefinitionBase *> arena_;
        NodeDefinitionBase *root_;
        ComponentContext *context_;
        NodeDefinitionBase *storeBase(const NodeDefinitionBase &def)
        {
          NodeDefinitionBase *cloned = def.clone();
          cloned->setCleanupHook(&NodeComposition::cleanupStoredNode, this);
          arena_.push_back(cloned);
          return cloned;
        }
        static void cleanupStoredNode(NodeDefinitionBase *node, void *context)
        {
          if (!node || !context)
          {
            return;
          }
          NodeComposition *self = static_cast<NodeComposition *>(context);
          self->releaseStoredNode(node);
        }
        void releaseStoredNode(NodeDefinitionBase *node)
        {
          if (!node)
          {
            return;
          }
          for (size_t i = 0; i < arena_.size(); ++i)
          {
            if (arena_[i] == node)
            {
              arena_.erase(arena_.begin() + i);
              break;
            }
          }
          if (root_ == node)
          {
            root_ = 0;
          }
        }

        void destroyArena()
        {
          for (size_t i = 0; i < arena_.size(); ++i)
          {
            NodeDefinitionBase *node = arena_[i];
            if (!node)
            {
              continue;
            }
            node->clearCleanupHook();
            delete node;
          }
          arena_.clear();
          root_ = 0;
        }

      public:
        NodeComposition() : root_(0), context_(0) {}

        ~NodeComposition() { this->destroyArena(); }

        // Store a copy of the definition in the arena and return pointer
        template <typename T>
        T *store(const T &def)
        {
          return static_cast<T *>(storeBase(def));
        }
        NodeDefinitionBase *store(const NodeDefinitionBase &def) { return storeBase(def); }

        // Declare root node
        template <typename T>
        T &declare(const T &def)
        {
          T *newRoot = this->store(def);
          this->root_ = newRoot;
          return *newRoot;
        }
        NodeDefinitionBase &declare(const NodeDefinitionBase &def)
        {
          NodeDefinitionBase *newRoot = this->store(def);
          this->root_ = newRoot;
          return *newRoot;
        }
        NodeDefinitionBase &declare(const INestableDefinition &def)
        {
          const NodeDefinitionBase *base = def.asNodeDefinitionBase();
          assert(base && "Nestable definitions must derive from NodeDefinitionBase");
          return this->declare(*base);
        }

        void setContext(ComponentContext *context) { context_ = context; }
        ComponentContext *componentContext() const { return context_; }
        const ComponentContext *attachedContext() const { return context_; }
        const ComponentContext &ensureAttached() const
        {
          assert(context_ && "NodeComposition::ensureAttached requires ComponentContext");
          assert(context_->boundary() && "NodeComposition::ensureAttached requires BoundaryNode");
          assert(context_->scene() && "NodeComposition::ensureAttached requires Scene");
          assert(context_->window() && "NodeComposition::ensureAttached requires Window");
          return *context_;
        }
        bool hasContext() const { return context_ != 0; }
        BoundaryNode *boundary() const;
        Scene *scene() const;
        ::Window *window() const;

        void reset()
        {
          this->destroyArena();
          context_ = 0;
        }

        // Create node tree
        Node *createNodeTree() const;

        NodeDefinitionBase *root() const { return root_; }

        // --- DSL helpers ---
        template <typename T>
        declara::core::scene::ConditionalDefinition conditional(const State<bool> &condition, T &x)
        {
          extern T Empty;
          return ConditionalDefinition(ConditionalProps(&condition, &x, &Empty));
        }

        template <typename T>
        declara::core::scene::ConditionalDefinition conditional(State<bool> &condition, T &x)
        {
          extern T Empty;
          return ConditionalDefinition(ConditionalProps(&condition, &x, &Empty));
        }

        template <typename T>
        StreamView<typename std::vector<T>::const_iterator> stream(const std::vector<T> &v)
        {
          return StreamView<typename std::vector<T>::const_iterator>(v.begin(), v.end());
        }

        template <typename It>
        StreamView<It> stream(It begin, It end)
        {
          return StreamView<It>(begin, end);
        }

        template <typename T>
        T *group(const T &x)
        {
          return this->store(x);
        }
        NodeDefinitionBase *group(const INestableDefinition &x)
        {
          const NodeDefinitionBase *base = x.asNodeDefinitionBase();
          assert(base && "Nestable definitions must derive from NodeDefinitionBase");
          return this->store(*base);
        }

        template <typename T>
        BoundState<T> useState(const T &initial)
        {
          assert(context_ && "NodeComposition::useState requires ComponentContext");
          IStateOwner *stateOwner = context_->stateOwner();
          assert(stateOwner && "NodeComposition::useState requires Boundary owner");
          MutableState<T> *state = new MutableState<T>(initial);
          // Use unchecked version - useState always creates new unique states
          stateOwner->adoptStateUnchecked(state);
          return BoundState<T>(state, stateOwner->tracker());
        }

        StateBatch declareStates()
        {
          return StateBatch(this);
        }

        template <typename T>
        BoundState<T> useState()
        {
          return useState(T());
        }

        template <typename T>
        T *findBoundary() const
        {
          if (!context_)
          {
            return 0;
          }
          ComponentContext *ctx = context_;
          while (ctx)
          {
            Node *owner = ctx->owner();
            if (owner)
            {
              T *typed = dynamic_cast<T *>(owner);
              if (typed)
              {
                return typed;
              }
            }
            ctx = ctx->parent();
          }
          return 0;
        }

      private:
        friend class StateBatch;
        void useStates(size_t count)
        {
          assert(context_ && "NodeComposition::useStates requires ComponentContext");
          IStateOwner *stateOwner = context_->stateOwner();
          assert(stateOwner && "NodeComposition::useStates requires Boundary owner");
          stateOwner->reserveStates(count);
        }
      };

      inline NodeComposition::StateBatch::StateBatch(NodeComposition *composition)
          : impl_(composition ? new Impl(composition) : 0) {}

      inline NodeComposition::StateBatch::StateBatch(const NodeComposition::StateBatch &other)
          : impl_(other.impl_)
      {
        if (impl_)
        {
          ++impl_->refs;
        }
      }

      inline NodeComposition::StateBatch &NodeComposition::StateBatch::operator=(const NodeComposition::StateBatch &other)
      {
        if (this == &other)
        {
          return *this;
        }
        release();
        impl_ = other.impl_;
        if (impl_)
        {
          ++impl_->refs;
        }
        return *this;
      }

      inline NodeComposition::StateBatch::~StateBatch()
      {
        release();
      }

      inline void NodeComposition::StateBatch::finalize()
      {
        if (!impl_ || impl_->done)
        {
          return;
        }
        impl_->done = true;
        if (impl_->composition && impl_->count > 0)
        {
          impl_->composition->useStates(impl_->count);
          for (size_t i = 0; i < impl_->specs.size(); ++i)
          {
            impl_->specs[i]->apply(impl_->composition);
          }
        }
        for (size_t i = 0; i < impl_->specs.size(); ++i)
        {
          delete impl_->specs[i];
        }
        impl_->specs.clear();
      }

      inline void NodeComposition::StateBatch::release()
      {
        if (!impl_)
        {
          return;
        }
        --impl_->refs;
        if (impl_->refs == 0)
        {
          finalize();
          delete impl_;
        }
        impl_ = 0;
      }

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // LOKA_CORE2_SCENE_NODECOMPOSITION_HPP
