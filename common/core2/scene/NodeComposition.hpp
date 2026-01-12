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

        // StateBatch: Builder パターンで State を収集し、デストラクタで一括作成
        class StateBatch
        {
        public:
          enum
          {
            kMaxStates = 16,
            kStorageBytes = 32
          };

          StateBatch(IStateOwner *owner)
              : owner_(owner), count_(0), totalBytes_(0) {}

          ~StateBatch()
          {
            if (count_ == 0 || !owner_)
            {
              return;
            }
            // 1回だけ確保
            owner_->reserveStateArena(totalBytes_);
            owner_->reserveStates(count_);
            // 各 state を作成
            for (size_t i = 0; i < count_; ++i)
            {
              entries_[i].create(owner_, entries_[i].out, entries_[i].storage.bytes);
            }
          }

          template <typename T>
          StateBatch &state(BoundState<T> &out, const T &initial)
          {
            if (count_ >= kMaxStates)
            {
              CreateImmediateState(owner_, out, initial);
              return *this;
            }
            if (sizeof(T) > kStorageBytes)
            {
              assert(false && "StateBatch::state only supports small state initializers");
              return *this;
            }
            Entry &e = entries_[count_++];
            e.out = &out;
            e.size = sizeof(MutableState<T>);
            e.align = __alignof__(MutableState<T>);
            e.create = &CreateState<T>;
            // 初期値をコピー（小さい値のみ inline）
            copyInitial<T>(e.storage.bytes, initial);
            totalBytes_ += e.size + e.align; // alignment 余裕
            return *this;
          }

        private:
          typedef void (*CreateFn)(IStateOwner *, void *, void *);

          struct Storage
          {
            double d;
            void *p;
            char bytes[kStorageBytes];
          };

          struct Entry
          {
            void *out;
            size_t size;
            size_t align;
            CreateFn create;
            Storage storage; // 初期値の inline storage
          };

          template <typename T>
          static void copyInitial(char *storage, const T &value)
          {
            new (storage) T(value);
          }

          template <typename T>
          static void CreateState(IStateOwner *owner, void *outPtr, void *initialPtr)
          {
            BoundState<T> *out = static_cast<BoundState<T> *>(outPtr);
            T *initial = reinterpret_cast<T *>(initialPtr);
            MutableState<T> *state = 0;
            size_t align = __alignof__(MutableState<T>);
            void *mem = owner->allocateStateMemory(sizeof(MutableState<T>), align);
            if (mem)
            {
              state = new (mem) MutableState<T>(*initial);
              state->setArenaAllocated(true);
              owner->registerStateMemory(state, &DestroyState<T>);
            }
            else
            {
              state = new MutableState<T>(*initial);
            }
            owner->adoptStateUnchecked(state);
            *out = BoundState<T>(state, owner->tracker(), owner);
            // 初期値のデストラクタ呼び出し
            initial->~T();
          }

        public:
          template <typename T>
          static void CreateImmediateState(IStateOwner *owner, BoundState<T> &out, const T &initial)
          {
            MutableState<T> *state = 0;
            size_t align = __alignof__(MutableState<T>);
            void *mem = owner ? owner->allocateStateMemory(sizeof(MutableState<T>), align) : 0;
            if (mem)
            {
              state = new (mem) MutableState<T>(initial);
              state->setArenaAllocated(true);
              owner->registerStateMemory(state, &DestroyState<T>);
            }
            else
            {
              state = new MutableState<T>(initial);
            }
            if (owner)
            {
              owner->adoptStateUnchecked(state);
              out = BoundState<T>(state, owner->tracker(), owner);
            }
            else
            {
              out = BoundState<T>(state, 0, 0);
            }
          }

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
          IStateOwner *owner_;
          Entry entries_[kMaxStates];
          size_t count_;
          size_t totalBytes_;
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
          return BoundState<T>(state, stateOwner->tracker(), stateOwner);
        }

        StateBatch declareStates()
        {
          assert(context_ && "NodeComposition::declareStates requires ComponentContext");
          IStateOwner *owner = context_->stateOwner();
          assert(owner && "NodeComposition::declareStates requires IStateOwner");
          return StateBatch(owner);
        }

        template <typename T>
        BoundState<T> useState()
        {
          return useState(T());
        }

        // findBoundary: T must implement static T* fromNode(Node*)
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
              T *typed = T::fromNode(owner);
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
        static NodeComposition *current_;
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // LOKA_CORE2_SCENE_NODECOMPOSITION_HPP
