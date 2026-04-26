#ifndef LOKA_CORE2_SCENE_NODE_COMPOSABLENODE_HPP
#define LOKA_CORE2_SCENE_NODE_COMPOSABLENODE_HPP

#include <cassert>
#include <cstring>
#include <vector>
#include "../Node.hpp"
#include "../ComponentContext.hpp"
#include "../NodeComposition.hpp"
#include "../StateOwner.hpp"
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

      enum ContextPlacement
      {
        CONTEXT_PLACEMENT_BOUNDARY = 0,
        CONTEXT_PLACEMENT_ROOT = 1
      };

      // Base class for nodes that build children via compose() when requested
      class ComposableNode : public NestableNode
      {
      public:
        ComposableNode() : currentContext_(0), nodeStateOwner_(0), isAttached_(false), attached_(), nodeStates_() {}
        virtual ~ComposableNode()
        {
          releaseCallbacks();
          clearChildren();
          releaseNodeStateRegistrations();
        }

        virtual ComposableNode *asComposable() { return this; }

        virtual void compose(ComponentContext &context, ComposeEvent event)
        {
          PROFILE_FUNC();
          {
            ContextScope scope(this, &context);
            attached_.boundary_ = context.boundary();
            attached_.scene_ = context.scene();
            attached_.window_ = context.window();
            isAttached_ = attached_.boundary_ && attached_.scene_ && attached_.window_;
            nodeStateOwner_ = context.stateOwner();
            if (event != COMPOSE_EVENT_DETACH)
            {
              this->connectNodeStateRegistrations();
            }
            {
              PROFILE_SECTION("compWith");
              this->composeWithContext(context, event);
            }
            if (event == COMPOSE_EVENT_DETACH)
            {
              attached_.reset();
              isAttached_ = false;
              nodeStateOwner_ = 0;
            }
          }
        }

        virtual void compose(ComponentContext &context)
        {
          this->compose(context, COMPOSE_EVENT_ATTACH);
        }

      protected:
        virtual void composeWithContext(ComponentContext &context, ComposeEvent event) = 0;
        virtual void attachNode(NodeComposition &c) { (void)c; }
        virtual void detachNode(NodeComposition &c)
        {
          (void)c;
          releaseCallbacks();
        }

        ComponentContext *componentContext() const { return currentContext_; }
        struct CallbackEntryBase
        {
          virtual ~CallbackEntryBase() {}
          virtual void unbind() = 0;
          virtual void invalidate() = 0;
          virtual bool matches(const void *source, void *node, const void *methodBytes, size_t methodSize) const = 0;
        };

        template <class NodeT>
        struct CallbackEntry : public CallbackEntryBase
        {
          typedef void (NodeT::*Method)();
          CallbackEntry(NodeT *node, loka::core::EmitterState *emitter, Method method)
              : node_(node), emitter_(emitter), method_(method), valid_(true) {}

          static void Invoke(void *userData)
          {
            CallbackEntry *self = static_cast<CallbackEntry *>(userData);
            if (!self || !self->valid_ || !self->node_)
            {
              return;
            }
            (self->node_->*(self->method_))();
          }

          void unbind()
          {
            if (emitter_)
            {
              emitter_->deferUnbind(&Invoke, this);
            }
          }

          void invalidate()
          {
            valid_ = false;
          }

          bool matches(const void *source, void *node, const void *methodBytes, size_t methodSize) const
          {
            if (emitter_ != source || node_ != node || methodSize != sizeof(method_))
            {
              return false;
            }
            return std::memcmp(&method_, methodBytes, sizeof(method_)) == 0;
          }

          NodeT *node_;
          loka::core::EmitterState *emitter_;
          Method method_;
          bool valid_;
        };

        template <class StateT, class NodeT>
        struct StateCallbackEntry : public CallbackEntryBase
        {
          typedef void (NodeT::*Method)();
          StateCallbackEntry(StateT *state, NodeT *node, Method method)
              : state_(state), node_(node), method_(method), valid_(true) {}

          static void Invoke(void *userData)
          {
            StateCallbackEntry *self = static_cast<StateCallbackEntry *>(userData);
            if (!self || !self->valid_ || !self->node_)
            {
              return;
            }
            (self->node_->*(self->method_))();
          }

          void unbind()
          {
            if (state_)
            {
              state_->unbind(&Invoke, this);
            }
          }

          void invalidate()
          {
            valid_ = false;
          }

          bool matches(const void *source, void *node, const void *methodBytes, size_t methodSize) const
          {
            if (state_ != source || node_ != node || methodSize != sizeof(method_))
            {
              return false;
            }
            return std::memcmp(&method_, methodBytes, sizeof(method_)) == 0;
          }

          StateT *state_;
          NodeT *node_;
          Method method_;
          bool valid_;
        };

        template <class NodeT>
        void bindActionForUi(loka::core::EmitterState &emitter, NodeT *node, void (NodeT::*method)())
        {
          for (size_t i = 0; i < callbacks_.size(); ++i)
          {
            if (callbacks_[i] && callbacks_[i]->matches(&emitter, node, &method, sizeof(method)))
            {
              return;
            }
          }
          CallbackEntry<NodeT> *entry = new CallbackEntry<NodeT>(node, &emitter, method);
          callbacks_.push_back(entry);
          emitter.deferBind(&CallbackEntry<NodeT>::Invoke, entry);
        }

        template <class NodeT>
        void bindActionForUi(loka::core::EmitterState &emitter, void (NodeT::*method)())
        {
          NodeT *self = static_cast<NodeT *>(this);
          this->bindActionForUi(emitter, self, method);
        }

        template <class NodeT>
        void bindForUi(loka::core::EmitterState &emitter, NodeT *node, void (NodeT::*method)())
        {
          this->bindActionForUi(emitter, node, method);
        }

        template <class NodeT>
        void bindForUi(loka::core::EmitterState &emitter, void (NodeT::*method)())
        {
          this->bindActionForUi(emitter, method);
        }

        template <class StateT, class NodeT>
        void watchStateForUi(StateT &state, NodeT *node, void (NodeT::*method)(), bool callImmediately = false)
        {
          for (size_t i = 0; i < callbacks_.size(); ++i)
          {
            if (callbacks_[i] && callbacks_[i]->matches(&state, node, &method, sizeof(method)))
            {
              return;
            }
          }
          StateCallbackEntry<StateT, NodeT> *entry = new StateCallbackEntry<StateT, NodeT>(&state, node, method);
          callbacks_.push_back(entry);
          state.bind(&StateCallbackEntry<StateT, NodeT>::Invoke, entry, callImmediately);
        }

        template <class StateT, class NodeT>
        void watchStateForUi(StateT &state, void (NodeT::*method)(), bool callImmediately = false)
        {
          NodeT *self = static_cast<NodeT *>(this);
          this->watchStateForUi(state, self, method, callImmediately);
        }

        template <class StateT, class NodeT>
        void bindStateForUi(StateT &state, NodeT *node, void (NodeT::*method)(), bool callImmediately = false)
        {
          this->watchStateForUi(state, node, method, callImmediately);
        }

        template <class StateT, class NodeT>
        void bindStateForUi(StateT &state, void (NodeT::*method)(), bool callImmediately = false)
        {
          this->watchStateForUi(state, method, callImmediately);
        }

        template <typename T>
        void state(NodeState<T> &out, const T &initial)
        {
          for (size_t i = 0; i < nodeStates_.size(); ++i)
          {
            if (nodeStates_[i] && nodeStates_[i]->matches(&out))
            {
              assert(false && "ComposableNode::state registered the same NodeState twice");
              return;
            }
          }
          NodeStateRegistration<T> *entry = new NodeStateRegistration<T>(&out, initial);
          nodeStates_.push_back(entry);
          this->connectNodeStateRegistration(entry);
        }

        class NodeStateBatch
        {
        public:
          enum
          {
            kStorageBytes = 32
          };

          NodeStateBatch(ComposableNode *node, size_t capacity)
              : node_(node), entries_(0), count_(0), capacity_(capacity)
          {
            if (capacity_ > 0)
            {
              entries_ = new Entry[capacity_];
            }
          }

          NodeStateBatch(const NodeStateBatch &other)
              : node_(other.node_), entries_(other.entries_), count_(other.count_), capacity_(other.capacity_)
          {
            other.node_ = 0;
            other.entries_ = 0;
            other.count_ = 0;
            other.capacity_ = 0;
          }

          ~NodeStateBatch()
          {
            if (node_ && entries_ && count_ > 0)
            {
              node_->addNodeStateBatchRegistration(entries_, count_);
              entries_ = 0;
              count_ = 0;
              capacity_ = 0;
              return;
            }
            if (entries_)
            {
              for (size_t i = 0; i < count_; ++i)
              {
                if (entries_[i].destroyInitial)
                {
                  entries_[i].destroyInitial(entries_[i]);
                }
              }
              delete[] entries_;
            }
          }

          template <typename T>
          NodeStateBatch &state(NodeState<T> &out, const T &initial)
          {
            assert(sizeof(T) <= kStorageBytes && "NodeStateBatch::state only supports small state initializers");
            if (sizeof(T) > kStorageBytes)
            {
              return *this;
            }
            assert(count_ < capacity_ && "NodeStateBatch capacity exceeded");
            if (count_ >= capacity_)
            {
              return *this;
            }
            Entry &entry = entries_[count_++];
            entry.out = &out;
            entry.connect = &ConnectState<T>;
            entry.disconnect = &DisconnectState<T>;
            entry.matches = &MatchesState<T>;
            entry.destroyInitial = &DestroyInitial<T>;
            new (entry.storage.bytes) T(initial);
            return *this;
          }

        private:
          struct Storage
          {
            double d;
            void *p;
            char bytes[kStorageBytes];
          };

        public:
          struct Entry
          {
            Entry()
                : out(0), owner(0), state(0), connect(0), disconnect(0), matches(0), destroyInitial(0), storage() {}
            void *out;
            IStateOwner *owner;
            loka::core::StateBase *state;
            void (*connect)(Entry &, IStateOwner *);
            void (*disconnect)(Entry &);
            bool (*matches)(const Entry &, const void *);
            void (*destroyInitial)(Entry &);
            Storage storage;
          };

        private:
          template <typename T>
          static bool MatchesState(const Entry &entry, const void *out)
          {
            return entry.out == out;
          }

          template <typename T>
          static void ConnectState(Entry &entry, IStateOwner *owner)
          {
            NodeState<T> *out = static_cast<NodeState<T> *>(entry.out);
            if (!out || !owner)
            {
              return;
            }
            if (out->isValid())
            {
              assert(out->dangerouslyOwner() == owner && "Node-local state reattached to a different owner");
              entry.owner = out->dangerouslyOwner();
              entry.state = out->dangerouslyMutableState();
              return;
            }
            T *initial = reinterpret_cast<T *>(entry.storage.bytes);
            NodeComposition::StateBatch::CreateImmediateState(owner, *out, *initial);
            entry.owner = out->dangerouslyOwner();
            entry.state = out->dangerouslyMutableState();
          }

          template <typename T>
          static void DisconnectState(Entry &entry)
          {
            if (entry.owner && entry.state)
            {
              entry.owner->releaseState(entry.state);
            }
            entry.owner = 0;
            entry.state = 0;
            entry.out = 0;
          }

          template <typename T>
          static void DestroyInitial(Entry &entry)
          {
            T *initial = reinterpret_cast<T *>(entry.storage.bytes);
            initial->~T();
          }

          NodeStateBatch &operator=(const NodeStateBatch &);

          mutable ComposableNode *node_;
          mutable Entry *entries_;
          mutable size_t count_;
          mutable size_t capacity_;
        };

        NodeStateBatch declareStates()
        {
          return NodeStateBatch(this, 16);
        }

        NodeStateBatch declareStates(size_t count)
        {
          return NodeStateBatch(this, count);
        }

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
          releaseCallbacks();
          composition_.reset();
          composition_.setContext(&context);
          return composition_;
        }

        NodeComposition &composition() { return composition_; }
        const NodeComposition &composition() const { return composition_; }

        void clearChildren()
        {
          clearChildrenInternal(false);
        }

      protected:
        struct NodeStateRegistrationBase
        {
          virtual ~NodeStateRegistrationBase() {}
          virtual bool matches(const void *out) const = 0;
          virtual void connect(IStateOwner *owner) = 0;
          virtual void disconnect() = 0;
        };

        template <typename T>
        struct NodeStateRegistration : public NodeStateRegistrationBase
        {
          NodeStateRegistration(NodeState<T> *out, const T &initial)
              : out_(out), initial_(initial), owner_(0), state_(0) {}

          bool matches(const void *out) const
          {
            return out_ == out;
          }

          void connect(IStateOwner *owner)
          {
            if (!out_ || !owner)
            {
              return;
            }
            if (out_->isValid())
            {
              assert(out_->dangerouslyOwner() == owner && "Node-local state reattached to a different owner");
              owner_ = out_->dangerouslyOwner();
              state_ = out_->dangerouslyMutableState();
              return;
            }
            NodeComposition::StateBatch::CreateImmediateState(owner, *out_, initial_);
            owner_ = out_->dangerouslyOwner();
            state_ = out_->dangerouslyMutableState();
          }

          void disconnect()
          {
            if (owner_ && state_)
            {
              owner_->releaseState(state_);
            }
            owner_ = 0;
            state_ = 0;
            out_ = 0;
          }

          NodeState<T> *out_;
          T initial_;
          IStateOwner *owner_;
          loka::core::MutableState<T> *state_;
        };

        struct NodeStateBatchRegistration : public NodeStateRegistrationBase
        {
          typedef NodeStateBatch::Entry Entry;

          NodeStateBatchRegistration(Entry *entries, size_t count)
              : entries_(entries), count_(count)
          {
          }

          ~NodeStateBatchRegistration()
          {
            if (entries_)
            {
              for (size_t i = 0; i < count_; ++i)
              {
                if (entries_[i].destroyInitial)
                {
                  entries_[i].destroyInitial(entries_[i]);
                }
              }
              delete[] entries_;
            }
          }

          bool matches(const void *out) const
          {
            for (size_t i = 0; i < count_; ++i)
            {
              if (entries_[i].matches && entries_[i].matches(entries_[i], out))
              {
                return true;
              }
            }
            return false;
          }

          void connect(IStateOwner *owner)
          {
            for (size_t i = 0; i < count_; ++i)
            {
              if (entries_[i].connect)
              {
                entries_[i].connect(entries_[i], owner);
              }
            }
          }

          void disconnect()
          {
            for (size_t i = 0; i < count_; ++i)
            {
              if (entries_[i].disconnect)
              {
                entries_[i].disconnect(entries_[i]);
              }
            }
          }

          Entry *entries_;
          size_t count_;
        };

        void addNodeStateBatchRegistration(NodeStateBatch::Entry *entries, size_t count)
        {
          if (!entries || count == 0)
          {
            return;
          }
          NodeStateBatchRegistration *entry = new NodeStateBatchRegistration(entries, count);
          nodeStates_.push_back(entry);
          this->connectNodeStateRegistration(entry);
        }

        void connectNodeStateRegistration(NodeStateRegistrationBase *entry)
        {
          if (!entry || !nodeStateOwner_)
          {
            return;
          }
          entry->connect(nodeStateOwner_);
        }

        void connectNodeStateRegistrations()
        {
          for (size_t i = 0; i < nodeStates_.size(); ++i)
          {
            this->connectNodeStateRegistration(nodeStates_[i]);
          }
        }

        void releaseNodeStateRegistrations()
        {
          for (size_t i = 0; i < nodeStates_.size(); ++i)
          {
            if (nodeStates_[i])
            {
              nodeStates_[i]->disconnect();
            }
            delete nodeStates_[i];
          }
          nodeStates_.clear();
        }

        void releaseCallbacks()
        {
          for (size_t i = 0; i < callbacks_.size(); ++i)
          {
            callbacks_[i]->unbind();
            callbacks_[i]->invalidate();
            delete callbacks_[i];
          }
          callbacks_.clear();
        }

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
              IStateOwner *currentOwner = n->asStateOwner();
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
        IStateOwner *nodeStateOwner_;
        NodeComposition composition_;
        bool isAttached_;
        AttachedContext attached_;
        std::vector<CallbackEntryBase *> callbacks_;
        std::vector<NodeStateRegistrationBase *> nodeStates_;
      };

    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_NODE_COMPOSABLENODE_HPP
