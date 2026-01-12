#ifndef LOKA_CORE2_SCENE_NODECOMPOSITION_HPP
#define LOKA_CORE2_SCENE_NODECOMPOSITION_HPP

#include <vector>
#include <cassert>
#include <new>
#include "core2/scene/Node.hpp"
#include "core2/scene/StreamView.hpp"
#include "core2/scene/node/Conditional.hpp"
#include "core2/scene/BoundState.hpp"
#include "core2/scene/ComponentContext.hpp"
#include "core2/scene/StateOwner.hpp"
#include "core/Profiler.hpp"

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
        struct CompositionScope
        {
          explicit CompositionScope(NodeComposition &composition);
          ~CompositionScope();
        private:
          NodeComposition *prev_;
        };

        class StateBatch
        {
        public:
          struct SpecBase
          {
            virtual ~SpecBase() {}
            virtual void apply(NodeComposition *composition) = 0;
          };

          enum
          {
            kMaxSpecs = 16,
            kMaxSpecBytes = 1024
          };

          struct Impl
          {
            Impl()
                : count(0),
                  bytes(0),
                  specCount(0),
                  specUsed(0)
            {
              for (size_t i = 0; i < kMaxSpecs; ++i)
              {
                specs[i] = 0;
              }
              for (size_t i = 0; i < kMaxSpecBytes; ++i)
              {
                specStorage.bytes[i] = 0;
              }
            }
            size_t count;
            size_t bytes;
            size_t specCount;
            size_t specUsed;
            SpecBase *specs[kMaxSpecs];
            struct SpecStorage
            {
              void *align;
              char bytes[kMaxSpecBytes];
            } specStorage;
          };
          explicit StateBatch(NodeComposition *composition, Impl *impl);
          StateBatch(const StateBatch &other);
          StateBatch &operator=(const StateBatch &other);
          ~StateBatch();

          template <typename T>
          StateBatch &state(BoundState<T> &out, const T &initial)
          {
            assert(composition_ && "StateBatch::state requires active batch");
            if (!impl_)
            {
              return *this;
            }
            assert(composition_->stateBatchActive_ && "StateBatch::state requires declareStates scope");
            if (impl_->specCount >= kMaxSpecs)
            {
              flush();
            }
            size_t specAlign = __alignof__(Spec<T>);
            size_t offset = alignUp(impl_->specUsed, specAlign);
            size_t needed = offset + sizeof(Spec<T>);
            if (needed > kMaxSpecBytes)
            {
              flush();
              offset = 0;
              needed = sizeof(Spec<T>);
            }
            Spec<T> *spec = new (impl_->specStorage.bytes + offset) Spec<T>(out, initial);
            impl_->specs[impl_->specCount++] = spec;
            impl_->specUsed = needed;
            ++impl_->count;
            size_t align = __alignof__(MutableState<T>);
            impl_->bytes += sizeof(MutableState<T>) + normalizeAlign(align);
            return *this;
          }

        private:
          template <typename T>
          struct Spec : public SpecBase
          {
            Spec(BoundState<T> &outRef, const T &value) : out(&outRef), initial(value) {}
            virtual void apply(NodeComposition *composition)
            {
              assert(composition && "StateBatch::apply requires composition");
              IStateOwner *stateOwner = composition->context_->stateOwner();
              assert(stateOwner && "StateBatch::apply requires Boundary owner");
              MutableState<T> *state = 0;
              size_t align = __alignof__(MutableState<T>);
              void *mem = stateOwner->allocateStateMemory(sizeof(MutableState<T>), align);
              if (mem)
              {
                state = new (mem) MutableState<T>(initial);
                state->setArenaAllocated(true);
                stateOwner->registerStateMemory(state, &StateBatch::DestroyState<T>);
              }
              else
              {
                state = new MutableState<T>(initial);
              }
              stateOwner->adoptStateUnchecked(state);
              *out = BoundState<T>(state, stateOwner->tracker());
            }
            BoundState<T> *out;
            T initial;
          };

          NodeComposition *composition_;
          Impl *impl_;
          void finalize();
          void release();
          void flush();
        public:
          static void flushImpl(NodeComposition *composition, Impl *impl);
        private:
        public:
          template <typename T>
          static void DestroyState(core::StateBase *state)
          {
            MutableState<T> *typed = static_cast<MutableState<T> *>(state);
            if (typed)
            {
              typed->~MutableState<T>();
            }
          }

        private:
          static size_t alignUp(size_t value, size_t align)
          {
            size_t mask = align - 1;
            return (value + mask) & ~mask;
          }

          static size_t normalizeAlign(size_t align)
          {
            size_t minAlign = sizeof(void *);
            if (minAlign < 2)
            {
              minAlign = 2;
            }
            if (align < minAlign)
            {
              align = minAlign;
            }
            if ((align & (align - 1)) != 0)
            {
              size_t p2 = 1;
              while (p2 < align)
              {
                p2 <<= 1;
              }
              align = p2;
            }
            return align;
          }
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
        NodeComposition() : root_(0), context_(0), stateBatchActive_(false), stateBatchRefs_(0), stateBatchImpl_() {}

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
        static NodeComposition *current();

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
          MutableState<T> *state = 0;
          size_t align = __alignof__(MutableState<T>);
          void *mem = stateOwner->allocateStateMemory(sizeof(MutableState<T>), align);
          if (mem)
          {
            state = new (mem) MutableState<T>(initial);
            state->setArenaAllocated(true);
            stateOwner->registerStateMemory(state, &StateBatch::DestroyState<T>);
          }
          else
          {
            state = new MutableState<T>(initial);
          }
          // Use unchecked version - useState always creates new unique states
          stateOwner->adoptStateUnchecked(state);
          return BoundState<T>(state, stateOwner->tracker());
        }

        StateBatch declareStates()
        {
          assert(context_ && "NodeComposition::declareStates requires ComponentContext");
          assert(!stateBatchActive_ && "NodeComposition::declareStates already in use");
          stateBatchActive_ = true;
          stateBatchRefs_ = 1;
          stateBatchImpl_.count = 0;
          stateBatchImpl_.bytes = 0;
          stateBatchImpl_.specCount = 0;
          stateBatchImpl_.specUsed = 0;
          return StateBatch(this, &stateBatchImpl_);
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
        void retainStateBatch();
        void releaseStateBatch();
        void useStates(size_t count)
        {
          assert(context_ && "NodeComposition::useStates requires ComponentContext");
          IStateOwner *stateOwner = context_->stateOwner();
          assert(stateOwner && "NodeComposition::useStates requires Boundary owner");
          stateOwner->reserveStates(count);
        }

        bool stateBatchActive_;
        int stateBatchRefs_;
        StateBatch::Impl stateBatchImpl_;
        static NodeComposition *current_;
      };

      inline NodeComposition::StateBatch::StateBatch(NodeComposition *composition, Impl *impl)
          : composition_(composition), impl_(impl)
      {
        if (composition_)
        {
        }
      }

      inline NodeComposition::StateBatch::StateBatch(const NodeComposition::StateBatch &other)
          : composition_(other.composition_), impl_(other.impl_)
      {
        if (composition_)
        {
          composition_->retainStateBatch();
        }
      }

      inline NodeComposition::StateBatch &NodeComposition::StateBatch::operator=(const NodeComposition::StateBatch &other)
      {
        if (this == &other)
        {
          return *this;
        }
        release();
        composition_ = other.composition_;
        impl_ = other.impl_;
        if (composition_)
        {
          composition_->retainStateBatch();
        }
        return *this;
      }

      inline NodeComposition::StateBatch::~StateBatch()
      {
        release();
      }

      inline void NodeComposition::StateBatch::finalize()
      {
        if (!impl_ || !composition_)
        {
          return;
        }
        flush();
      }

      inline void NodeComposition::StateBatch::flush()
      {
        flushImpl(composition_, impl_);
      }

      inline void NodeComposition::StateBatch::flushImpl(NodeComposition *composition, Impl *impl)
      {
        if (!composition || !impl || impl->specCount == 0)
        {
          return;
        }
        if (!composition->context_)
        {
          impl->specCount = 0;
          impl->specUsed = 0;
          impl->count = 0;
          impl->bytes = 0;
          return;
        }
        if (impl->count > 0)
        {
          IStateOwner *stateOwner = composition->context_->stateOwner();
          if (stateOwner && impl->bytes > 0)
          {
            stateOwner->reserveStateArena(impl->bytes);
          }
          composition->useStates(impl->count);
          for (size_t i = 0; i < impl->specCount; ++i)
          {
            impl->specs[i]->apply(composition);
            impl->specs[i]->~SpecBase();
            impl->specs[i] = 0;
          }
        }
        impl->specCount = 0;
        impl->specUsed = 0;
        impl->count = 0;
        impl->bytes = 0;
      }

      inline void NodeComposition::StateBatch::release()
      {
        if (!composition_)
        {
          return;
        }
        composition_->releaseStateBatch();
        composition_ = 0;
        impl_ = 0;
      }

      inline void NodeComposition::retainStateBatch()
      {
        ++stateBatchRefs_;
      }

      inline void NodeComposition::releaseStateBatch()
      {
        if (stateBatchRefs_ == 0)
        {
          return;
        }
        --stateBatchRefs_;
        if (stateBatchRefs_ == 0)
        {
          StateBatch::flushImpl(this, &stateBatchImpl_);
          stateBatchActive_ = false;
          stateBatchImpl_.count = 0;
          stateBatchImpl_.bytes = 0;
          stateBatchImpl_.specCount = 0;
          stateBatchImpl_.specUsed = 0;
        }
      }

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // LOKA_CORE2_SCENE_NODECOMPOSITION_HPP
