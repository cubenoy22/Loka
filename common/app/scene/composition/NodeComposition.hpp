#ifndef LOKA_CORE2_SCENE_COMPOSITION_NODECOMPOSITION_HPP
#define LOKA_CORE2_SCENE_COMPOSITION_NODECOMPOSITION_HPP

#include <vector>
#include <cassert>
#include <new>
#include "app/scene/Node.hpp"
#include "app/scene/node/Conditional.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/scene/state/NodeState.hpp"
#include "app/scene/state/StateBatchBase.hpp"
#include "app/scene/context/ComponentContext.hpp"
#include "app/scene/state/StateOwner.hpp"
#include "core/Profiler.hpp"
#include "core/util/OwnedDef.hpp"

class Window;

namespace loka
{
  namespace app
  {
    namespace scene
    {
      class BoundaryNode;
      class Scene;
      namespace testing
      {
        struct NodeCompositionTestAccess;
      }

      /** Completed node-materialization fact. allocationFailed is the
          monotonic OR of allocation refusals across the whole subtree. */
      struct NodeMaterializationResult
      {
        Node *root;
        bool allocationFailed;
      };

      struct NodeComposition
      {
      public:
        template <typename T> class FoundBoundary
        {
        public:
          FoundBoundary()
              : facade_(0)
          {
          }
          explicit FoundBoundary(const T *facade)
              : facade_(facade)
          {
          }

          bool isValid() const
          {
            return facade_ != 0;
          }
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
          template <typename T> class CurrentState
          {
          public:
            CurrentState()
                : state_(0),
                  owned_(false)
            {
            }
            CurrentState(NodeState<T> *state, bool owned)
                : state_(state),
                  owned_(owned)
            {
            }

            bool isValid() const
            {
              return state_ && owned_ && state_->isValid();
            }
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

          CurrentBoundary()
              : owner_(0)
          {
          }
          explicit CurrentBoundary(IStateOwner *owner)
              : owner_(owner)
          {
          }

          bool isValid() const
          {
            return owner_ != 0;
          }
          template <typename T> CurrentState<T> state(NodeState<T> &nodeState) const
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
              : composition_(composition),
                prev_(composition.activeParent_)
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
              : composition_(composition),
                scope_(composition, parent)
          {
          }

          operator NodeComposition &()
          {
            return composition_;
          }
          NodeComposition &composition()
          {
            return composition_;
          }

        private:
          NodeComposition &composition_;
          ParentScope scope_;
        };

        // StateBatch is a declaration transaction: it collects State declarations
        // and commits them as one owner-side batch when the batch leaves scope --
        // the end of the declaring statement for the usual temporary chain, the
        // end of the block for a named batch filled in a loop. This is the same
        // scope-guard idiom as CompositionScope and the tracker transaction
        // guards; the destructor is the commit hook because it is the only
        // end-of-use signal C++98 can enforce, and an explicit terminal call
        // could be forgotten silently. Storage mechanics live in PageList below.
        class StateBatch : private StateBatchBase
        {
        public:
          enum
          {
            kMaxStates = 16
          };

          StateBatch(IStateOwner *owner)
              : owner_(owner)
          {
          }

          // Copying is defined only for an empty batch: declareStates() returns
          // by value, which formally requires an accessible copy constructor even
          // though compilers elide the copy. A loaded source still asserts in
          // debug because it is an unintended path. Release builds flush the
          // source batch at the copy point and restart the copy empty: no
          // declarations are lost, but the transaction splits at the copy site.
          StateBatch(const StateBatch &other)
              : owner_(other.owner_)
          {
            assert(other.pages_.stateCount() == 0 &&
                   "copying a StateBatch with pending declarations is not supported");
            // Return-by-value formally requires an accessible copy ctor in C++98.
            // If a loaded batch is copied anyway, flush the source once here and
            // restart the copy empty rather than widening mutability for every
            // const call site.
            const_cast<StateBatch &>(other).flushPendingDeclarations();
          }

          ~StateBatch()
          {
            this->flushPendingDeclarations();
          }

          template <typename T> StateBatch &state(NodeState<T> &out, const T &initial)
          {
            typedef char LokaStateBatchInitializerTooLarge[(sizeof(T) <= kStorageBytes) ? 1 : -1];
            (void)sizeof(LokaStateBatchInitializerTooLarge);
            // First declaration wins, matching NodeStateBatch's duplicate guard.
            if (pages_.contains(&out))
            {
              assert(false && "StateBatch::state declared the same NodeState twice");
              return *this;
            }
            Entry &e = pages_.append(ArenaBytesForState<T>());
            e.out = &out;
            e.create = &CreateState<T>;
            CopyInitial<T>(e.storage.bytes, initial);
            return *this;
          }

        private:
          typedef void (*CreateFn)(IStateOwner *, void *, void *);

          struct Entry
          {
            void *out;
            CreateFn create;
            Storage storage; // inline storage for the initial value
          };

          struct Page
          {
            Page()
                : count(0),
                  next(0)
            {
            }

            Entry entries[kMaxStates];
            size_t count;
            Page *next;
          };

          // Owns the page chain and the reserve totals. The first page lives
          // inline, so batches of up to kMaxStates declarations never touch the
          // heap; overflow chains one heap page per kMaxStates and is freed on
          // destruction whether or not creation ran.
          class PageList
          {
          public:
            PageList()
                : tail_(&first_),
                  stateCount_(0),
                  arenaBytes_(0)
            {
            }

            ~PageList()
            {
              this->reset();
            }

            Entry &append(size_t reserveBytes)
            {
              if (tail_->count >= kMaxStates)
              {
                Page *page = new Page();
                tail_->next = page;
                tail_ = page;
              }
              ++stateCount_;
              arenaBytes_ += reserveBytes;
              return tail_->entries[tail_->count++];
            }

            Page *first()
            {
              return &first_;
            }
            bool contains(const void *outPtr) const
            {
              for (const Page *p = &first_; p; p = p->next)
              {
                for (size_t i = 0; i < p->count; ++i)
                {
                  if (p->entries[i].out == outPtr)
                  {
                    return true;
                  }
                }
              }
              return false;
            }
            size_t stateCount() const
            {
              return stateCount_;
            }
            size_t arenaBytes() const
            {
              return arenaBytes_;
            }
            void reset()
            {
              Page *p = first_.next;
              while (p)
              {
                Page *next = p->next;
                delete p;
                p = next;
              }
              first_.count = 0;
              first_.next = 0;
              tail_ = &first_;
              stateCount_ = 0;
              arenaBytes_ = 0;
            }

          private:
            // Not defined: the chain owns heap pages, so memberwise copy would
            // double-free. StateBatch's empty-only copy never copies its list.
            PageList(const PageList &);
            PageList &operator=(const PageList &);

            Page first_;
            Page *tail_;
            size_t stateCount_;
            size_t arenaBytes_;
          };

          template <typename T> static void CreateState(IStateOwner *owner, void *outPtr, void *initialPtr)
          {
            NodeState<T> *out = static_cast<NodeState<T> *>(outPtr);
            T *initial = reinterpret_cast<T *>(initialPtr);
            CreateStateFromInitial<T>(owner, *out, *initial);
            DestroyInitialObject<T>(initial);
          }

          // The commit half of the transaction: reserve owner storage once for
          // every declaration, then create in declaration order, so arena
          // locality and deferred creation hold regardless of count. Reserve
          // requires a live owner; create handles a null owner via the same
          // degenerate path CreateStateFromInitial always had.
          void flushPendingDeclarations()
          {
            if (pages_.stateCount() != 0 && owner_)
            {
              // A refused reservation is survivable: each creation below
              // still has the heap door, and CreateStateFromInitial raises
              // the owner's white flag when both doors refuse (#132 ruling
              // 3). Reservation refusal alone is degradation, not failure.
              (void)owner_->reserveStateArena(pages_.arenaBytes());
              owner_->reserveStates(pages_.stateCount());
            }
            for (Page *p = pages_.first(); p; p = p->next)
            {
              for (size_t i = 0; i < p->count; ++i)
              {
                p->entries[i].create(owner_, p->entries[i].out, p->entries[i].storage.bytes);
              }
            }
            pages_.reset();
          }

          IStateOwner *owner_;
          PageList pages_;
        };

      private:
        // Arena: owns copies of all definitions created during compose
        std::vector<NodeDefinitionBase *> arena_;
        NodeDefinitionBase *root_;
        INestableDefinition *activeParent_;
        ComponentContext *context_;
        NodeDefinitionBase *storeBase(const NodeDefinitionBase &def)
        {
          loka::core::OwnedDef<NodeDefinitionBase> cloned(def.clone());
          if (!cloned.isSet())
          {
            return 0;
          }
          cloned->setCleanupHook(&NodeComposition::cleanupStoredNode, this);
          NodeDefinitionBase *stored = cloned.get();
          arena_.push_back(cloned.take());
          return stored;
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
        NodeComposition()
            : root_(0),
              activeParent_(0),
              context_(0)
        {
        }

        ~NodeComposition()
        {
          this->destroyArena();
        }

        // Store a copy of the definition in the arena and return pointer
        template <typename T> T *store(const T &def)
        {
          return static_cast<T *>(storeBase(def));
        }
        NodeDefinitionBase *store(const NodeDefinitionBase &def)
        {
          return storeBase(def);
        }

        // Declare root node
        template <typename T> T &declare(const T &def)
        {
          if (activeParent_)
          {
            (*activeParent_) << const_cast<T &>(def);
            return const_cast<T &>(def);
          }
          T *newRoot = this->store(def);
          if (!newRoot)
          {
            return const_cast<T &>(def);
          }
          this->root_ = newRoot;
          return *newRoot;
        }

        template <typename T> T &declareTagged(NodeTag tag, const T &def)
        {
          T tagged(def);
          tagged.tag(tag);
          T &declared = this->declare(tagged);
          if (&declared == &tagged)
          {
            // declare() handed back our stack-local copy (parent path or clone
            // failure); returning it would dangle past this frame.
            return const_cast<T &>(def);
          }
          return declared;
        }
        NodeDefinitionBase &declare(const NodeDefinitionBase &def)
        {
          if (activeParent_)
          {
            (*activeParent_) << const_cast<NodeDefinitionBase &>(def);
            return const_cast<NodeDefinitionBase &>(def);
          }
          NodeDefinitionBase *newRoot = this->store(def);
          if (!newRoot)
          {
            return const_cast<NodeDefinitionBase &>(def);
          }
          this->root_ = newRoot;
          return *newRoot;
        }
        NodeDefinitionBase &declareTagged(NodeTag tag, const NodeDefinitionBase &def)
        {
          loka::core::OwnedDef<NodeDefinitionBase> tagged(def.clone());
          if (!tagged.isSet())
          {
            return const_cast<NodeDefinitionBase &>(def);
          }
          tagged->setNodeTag(tag);
          NodeDefinitionBase *stored = tagged.get();
          if (activeParent_)
          {
            activeParent_->addOwnedChild(tagged.take());
            return *stored;
          }
          tagged->setCleanupHook(&NodeComposition::cleanupStoredNode, this);
          arena_.push_back(tagged.take());
          this->root_ = stored;
          return *stored;
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

        void setContext(ComponentContext *context)
        {
          context_ = context;
        }
        ComponentContext *componentContext() const
        {
          return context_;
        }
        const ComponentContext *attachedContext() const
        {
          return context_;
        }
        const ComponentContext &ensureAttached() const
        {
          assert(context_ && "NodeComposition::ensureAttached requires ComponentContext");
          assert(context_->boundary() && "NodeComposition::ensureAttached requires BoundaryNode");
          assert(context_->scene() && "NodeComposition::ensureAttached requires Scene");
          assert(context_->window() && "NodeComposition::ensureAttached requires Window");
          return *context_;
        }
        bool hasContext() const
        {
          return context_ != 0;
        }
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
        void assignCompositionSeatSlots();

        NodeDefinitionBase *root() const
        {
          return root_;
        }

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

        template <typename T> ConditionalDefinition conditional(const loka::core::State<bool> &condition, T &x)
        {
          static loka::app::F emptyDef;
          return this->conditional(condition, x, emptyDef);
        }

        template <typename T> ConditionalDefinition conditional(loka::core::State<bool> &condition, T &x)
        {
          static loka::app::F emptyDef;
          return this->conditional(condition, x, emptyDef);
        }

        template <typename T> T *group(const T &x)
        {
          return this->store(x);
        }
        NodeDefinitionBase *group(const INestableDefinition &x)
        {
          const NodeDefinitionBase *base = x.asNodeDefinitionBase();
          assert(base && "Nestable definitions must derive from NodeDefinitionBase");
          return this->store(*base);
        }

        template <typename T> NodeState<T> dangerouslyUseState(const T &initial)
        {
          assert(context_ && "NodeComposition::dangerouslyUseState requires ComponentContext");
          IStateOwner *stateOwner = context_->stateOwner();
          assert(stateOwner && "NodeComposition::dangerouslyUseState requires Boundary owner");
          NodeState<T> state;
          StateBatchBase::CreateImmediateState<T>(stateOwner, state, initial);
          return state;
        }

        StateBatch declareStates()
        {
          assert(context_ && "NodeComposition::declareStates requires ComponentContext");
          IStateOwner *owner = context_->stateOwner();
          assert(owner && "NodeComposition::declareStates requires IStateOwner");
          return StateBatch(owner);
        }

        template <typename T> NodeState<T> dangerouslyUseState()
        {
          return dangerouslyUseState(T());
        }

        // findBoundary: T must implement static T* fromNode(Node*)
        template <typename T> FoundBoundary<T> findBoundary() const
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
        /** Internal materialization primitive. The returned allocation flag
            must be consumed by the operation's owner-side choke point. */
        NodeMaterializationResult createNodeFromDefinitionResult(
            NodeDefinitionBase *definition) const;

        friend class BoundaryNode;
        friend struct testing::NodeCompositionTestAccess;
        static NodeComposition *current_;
      };

#ifdef TEST_BUILD
      namespace testing
      {
        struct NodeCompositionTestAccess
        {
          static NodeMaterializationResult createNodeFromDefinitionResult(
              const NodeComposition &composition,
              NodeDefinitionBase *definition)
          {
            return composition.createNodeFromDefinitionResult(definition);
          }
        };
      }
#endif

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_COMPOSITION_NODECOMPOSITION_HPP
