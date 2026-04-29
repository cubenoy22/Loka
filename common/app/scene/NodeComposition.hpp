#ifndef LOKA_CORE2_SCENE_NODECOMPOSITION_HPP
#define LOKA_CORE2_SCENE_NODECOMPOSITION_HPP

#include <vector>
#include <cassert>
#include <new>
#include "app/scene/Node.hpp"
#include "app/scene/StreamView.hpp"
#include "app/scene/node/Conditional.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/scene/NodeState.hpp"
#include "app/scene/ComponentContext.hpp"
#include "app/scene/StateOwner.hpp"
#include "loka/core/Profiler.hpp"

class Window;

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class BoundaryNode;
      class Scene;

      struct NodeComposition
      {
      public:
        template <typename T>
        class FoundBoundary
        {
        public:
          FoundBoundary() : facade_(0) {}
          explicit FoundBoundary(const T *facade) : facade_(facade) {}

          bool isValid() const { return facade_ != 0; }
          const T &facade() const
          {
            assert(facade_ && "FoundBoundary::facade requires a boundary");
            return *facade_;
          }

        private:
          const T *facade_;
        };

        class CurrentBoundary
        {
        public:
          template <typename T>
          class CurrentState
          {
          public:
            CurrentState() : state_(0), owned_(false) {}
            CurrentState(NodeState<T> *state, bool owned) : state_(state), owned_(owned) {}

            bool isValid() const { return state_ && owned_ && state_->isValid(); }
            T get() const
            {
              assert(this->isValid() && "CurrentState::get requires owner-matched NodeState");
              return state_->get();
            }
            void set(const T &value, bool forceUpdate = false) const
            {
              assert(this->isValid() && "CurrentState::set requires owner-matched NodeState");
              state_->set(value, forceUpdate);
            }

          private:
            NodeState<T> *state_;
            bool owned_;
          };

          CurrentBoundary() : owner_(0) {}
          explicit CurrentBoundary(IStateOwner *owner) : owner_(owner) {}

          bool isValid() const { return owner_ != 0; }
          template <typename T>
          CurrentState<T> state(NodeState<T> &nodeState) const
          {
            return CurrentState<T>(&nodeState, owner_ && nodeState.dangerouslyOwner() == owner_);
          }

        private:
          IStateOwner *owner_;
        };

        struct CompositionScope
        {
          explicit CompositionScope(NodeComposition &composition);
          ~CompositionScope();
        private:
          NodeComposition *prev_;
        };

        struct ParentScope
        {
          explicit ParentScope(NodeComposition &composition, INestableDefinition &parent)
              : composition_(composition), prev_(composition.activeParent_)
          {
            composition_.activeParent_ = &parent;
          }
          ~ParentScope()
          {
            composition_.activeParent_ = prev_;
          }
        private:
          NodeComposition &composition_;
          INestableDefinition *prev_;
        };

        struct ChildComposition
        {
          explicit ChildComposition(NodeComposition &composition, INestableDefinition &parent)
              : composition_(composition), scope_(composition, parent) {}

          operator NodeComposition &() { return composition_; }
          NodeComposition &composition() { return composition_; }

        private:
          NodeComposition &composition_;
          ParentScope scope_;
        };

        class StateBatchBase
        {
        public:
          enum
          {
            kStorageBytes = 32
          };

          struct Storage
          {
            double d;
            void *p;
            char bytes[kStorageBytes];
          };

          template <typename T>
          static void CopyInitial(char *storage, const T &value)
          {
            new (storage) T(value);
          }

          template <typename T>
          static void DestroyInitialObject(void *initialPtr)
          {
            T *initial = reinterpret_cast<T *>(initialPtr);
            initial->~T();
          }

          template <typename T>
          static void DestroyState(loka::core::StateBase *state)
          {
            loka::core::MutableState<T> *typed = static_cast<loka::core::MutableState<T> *>(state);
            if (typed)
            {
              typedef loka::core::MutableState<T> MutableStateType;
              typed->~MutableStateType();
            }
          }

          template <typename T>
          static void CreateStateFromInitial(IStateOwner *owner, NodeState<T> &out, const T &initial)
          {
            loka::core::MutableState<T> *state = 0;
            size_t align = AlignOf<loka::core::MutableState<T> >::value;
            void *mem = owner ? owner->allocateStateMemory(sizeof(loka::core::MutableState<T>), align) : 0;
            if (mem)
            {
              state = new (mem) loka::core::MutableState<T>(initial);
              state->setArenaAllocated(true);
              owner->registerStateMemory(state, &DestroyState<T>);
            }
            else
            {
              state = new loka::core::MutableState<T>(initial);
            }
            if (owner)
            {
              owner->adoptStateUnchecked(state);
              out = NodeState<T>(state, owner->tracker(), owner);
            }
            else
            {
              out = NodeState<T>(state, 0, 0);
            }
          }

          template <typename T>
          static void CreateImmediateState(IStateOwner *owner, NodeState<T> &out, const T &initial)
          {
            CreateStateFromInitial<T>(owner, out, initial);
          }
        };

        // StateBatch collects State declarations and creates them as one owner-side batch.
        class StateBatch : public StateBatchBase
        {
        public:
          enum
          {
            kMaxStates = 16
          };

          StateBatch(IStateOwner *owner)
              : owner_(owner), count_(0), totalBytes_(0) {}

          ~StateBatch()
          {
            if (count_ == 0 || !owner_)
            {
              return;
            }
            // Reserve once for all declarations in this batch.
            owner_->reserveStateArena(totalBytes_);
            owner_->reserveStates(count_);
            // Create each state after the owner has reserved enough storage.
            for (size_t i = 0; i < count_; ++i)
            {
              entries_[i].create(owner_, entries_[i].out, entries_[i].storage.bytes);
            }
          }

          template <typename T>
          StateBatch &state(NodeState<T> &out, const T &initial)
          {
            if (count_ >= kMaxStates)
            {
              CreateImmediateState(owner_, out, initial);
              return *this;
            }
            typedef char LokaStateBatchInitializerTooLarge[(sizeof(T) <= kStorageBytes) ? 1 : -1];
            (void)sizeof(LokaStateBatchInitializerTooLarge);
            Entry &e = entries_[count_++];
            e.out = &out;
            e.size = sizeof(loka::core::MutableState<T>);
            e.align = AlignOf<loka::core::MutableState<T> >::value;
            e.create = &CreateState<T>;
            CopyInitial<T>(e.storage.bytes, initial);
            totalBytes_ += e.size + e.align; // spare bytes for alignment
            return *this;
          }

        private:
          typedef void (*CreateFn)(IStateOwner *, void *, void *);

          struct Entry
          {
            void *out;
            size_t size;
            size_t align;
            CreateFn create;
            Storage storage; // inline storage for the initial value
          };

          template <typename T>
          static void CreateState(IStateOwner *owner, void *outPtr, void *initialPtr)
          {
            NodeState<T> *out = static_cast<NodeState<T> *>(outPtr);
            T *initial = reinterpret_cast<T *>(initialPtr);
            CreateStateFromInitial<T>(owner, *out, *initial);
            DestroyInitialObject<T>(initial);
          }

          IStateOwner *owner_;
          Entry entries_[kMaxStates];
          size_t count_;
          size_t totalBytes_;
        };

      private:
        // Arena: owns copies of all definitions created during compose
        std::vector<NodeDefinitionBase *> arena_;
        NodeDefinitionBase *root_;
        INestableDefinition *activeParent_;
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
        NodeComposition() : root_(0), activeParent_(0), context_(0) {}

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
          if (activeParent_)
          {
            (*activeParent_) << const_cast<T &>(def);
            return const_cast<T &>(def);
          }
          T *newRoot = this->store(def);
          this->root_ = newRoot;
          return *newRoot;
        }

        template <typename T>
        T &declareTagged(NodeTag tag, const T &def)
        {
          T tagged(def);
          tagged.tag(tag);
          return this->declare(tagged);
        }
        NodeDefinitionBase &declare(const NodeDefinitionBase &def)
        {
          if (activeParent_)
          {
            (*activeParent_) << const_cast<NodeDefinitionBase &>(def);
            return const_cast<NodeDefinitionBase &>(def);
          }
          NodeDefinitionBase *newRoot = this->store(def);
          this->root_ = newRoot;
          return *newRoot;
        }
        NodeDefinitionBase &declareTagged(NodeTag tag, const NodeDefinitionBase &def)
        {
          NodeDefinitionBase *tagged = def.clone();
          tagged->setNodeTag(tag);
          if (activeParent_)
          {
            activeParent_->addOwnedChild(tagged);
            return *tagged;
          }
          tagged->setCleanupHook(&NodeComposition::cleanupStoredNode, this);
          arena_.push_back(tagged);
          this->root_ = tagged;
          return *tagged;
        }
        NodeDefinitionBase &declare(const INestableDefinition &def)
        {
          const NodeDefinitionBase *base = def.asNodeDefinitionBase();
          assert(base && "Nestable definitions must derive from NodeDefinitionBase");
          return this->declare(*base);
        }
        NodeDefinitionBase &declareTagged(NodeTag tag, const INestableDefinition &def)
        {
          const NodeDefinitionBase *base = def.asNodeDefinitionBase();
          assert(base && "Nestable definitions must derive from NodeDefinitionBase");
          return this->declareTagged(tag, *base);
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
        CurrentBoundary currentBoundary() const
        {
          return CurrentBoundary(context_ ? context_->stateOwner() : 0);
        }
        Scene *scene() const;
        ::Window *window() const;
        static NodeComposition *current();

        void reset()
        {
          this->destroyArena();
          activeParent_ = 0;
          context_ = 0;
        }

        // Create node tree
        Node *createNodeTree() const;
        Node *createNodeFromDefinition(NodeDefinitionBase *definition) const;

        NodeDefinitionBase *root() const { return root_; }

        // --- DSL helpers ---
        template <typename TTrue, typename TFalse>
        ConditionalDefinition conditional(const loka::core::State<bool> &condition, TTrue &trueDef, TFalse &falseDef)
        {
          return ConditionalDefinition(ConditionalProps(&condition, &trueDef, &falseDef));
        }

        template <typename TTrue, typename TFalse>
        ConditionalDefinition conditional(loka::core::State<bool> &condition, TTrue &trueDef, TFalse &falseDef)
        {
          return ConditionalDefinition(ConditionalProps(&condition, &trueDef, &falseDef));
        }

        template <typename T>
        ConditionalDefinition conditional(const loka::core::State<bool> &condition, T &x)
        {
          static loka::app::F emptyDef;
          return this->conditional(condition, x, emptyDef);
        }

        template <typename T>
        ConditionalDefinition conditional(loka::core::State<bool> &condition, T &x)
        {
          static loka::app::F emptyDef;
          return this->conditional(condition, x, emptyDef);
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
        NodeState<T> dangerouslyUseState(const T &initial)
        {
          assert(context_ && "NodeComposition::dangerouslyUseState requires ComponentContext");
          IStateOwner *stateOwner = context_->stateOwner();
          assert(stateOwner && "NodeComposition::dangerouslyUseState requires Boundary owner");
          NodeState<T> state;
          StateBatchBase::CreateStateFromInitial<T>(stateOwner, state, initial);
          return state;
        }

        StateBatch declareStates()
        {
          assert(context_ && "NodeComposition::declareStates requires ComponentContext");
          IStateOwner *owner = context_->stateOwner();
          assert(owner && "NodeComposition::declareStates requires IStateOwner");
          return StateBatch(owner);
        }

        template <typename T>
        NodeState<T> dangerouslyUseState()
        {
          return dangerouslyUseState(T());
        }

        // findBoundary: T must implement static T* fromNode(Node*)
        template <typename T>
        FoundBoundary<T> findBoundary() const
        {
          if (!context_)
          {
            return FoundBoundary<T>();
          }
          BoundaryNode *currentBoundary = context_->boundary();
          ComponentContext *ctx = context_->parent();
          while (ctx)
          {
            if (ctx->boundary() != currentBoundary)
            {
              Node *owner = ctx->owner();
              return FoundBoundary<T>(owner ? T::fromNode(owner) : 0);
            }
            ctx = ctx->parent();
          }
          return FoundBoundary<T>();
        }

      private:
        static NodeComposition *current_;
      };

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_NODECOMPOSITION_HPP
