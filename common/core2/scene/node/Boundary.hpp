#ifndef DECLARA_CORE2_SCENE_NODE_BOUNDARY_HPP
#define DECLARA_CORE2_SCENE_NODE_BOUNDARY_HPP

#include <cstdarg>
#include <vector>
#include "../Node.hpp"
#include "ComposableNode.hpp"
#include "../ComponentContext.hpp"
#include "core/Managed.hpp"
#include "core/StateTracker.hpp"
#include "core/util/StateUtil.hpp"

namespace declara
{
  namespace core
  {
    namespace scene
    {
      // BoundaryNode: owns a local tracker for its subtree.
      class BoundaryNode : public ComposableNode
      {
      public:
        BoundaryNode() : ComposableNode(), tracker_() {}
        virtual ~BoundaryNode()
        {
          clearOwnedStates();
          clearOwnedStateHandles();
        }

        declara::core::StateTracker *tracker() { return &tracker_; }

        template <class T>
        MutableState<T> &useState()
        {
          return useStateWithValue(T());
        }

        template <class T>
        MutableState<T> &useState(const T &initial)
        {
          return useStateWithValue(initial);
        }

        template <class T>
        Managed<MutableState<T> > useManagedState()
        {
          return useManagedStateWithValue(T());
        }

        template <class T>
        Managed<MutableState<T> > useManagedState(const T &initial)
        {
          return useManagedStateWithValue(initial);
        }

      protected:
        static void composeTree(Node *node, ComponentContext &parentContext, ComposeEvent event)
        {
          if (!node)
          {
            return;
          }
          ComposableNode *composable = dynamic_cast<ComposableNode *>(node);
          ComponentContext *contextForChildren = &parentContext;
          ComponentContext nodeContext(&parentContext);
          if (composable)
          {
            composable->compose(nodeContext, event);
            contextForChildren = &nodeContext;
          }
          INestable *nestable = dynamic_cast<INestable *>(node);
          if (!nestable)
          {
            return;
          }
          const std::vector<Node *> &children = nestable->getChildren();
          for (size_t i = 0; i < children.size(); ++i)
          {
            composeTree(children[i], *contextForChildren, event);
          }
        }

        void registerState(StateBase *state)
        {
          tracker_.addState(state);
        }

        void registerStates(StateBase *first, ...)
        {
          StateVector states;
          va_list args;
          va_start(args, first);
          for (StateBase *s = first; s != 0; s = va_arg(args, StateBase *))
          {
            if (s)
            {
              states.push_back(s);
            }
          }
          va_end(args);
          for (size_t i = 0; i < states.size(); ++i)
          {
            tracker_.addState(states[i]);
          }
        }

      private:
        struct StateHandleBase
        {
          virtual ~StateHandleBase() {}
          virtual StateBase *state() const = 0;
        };

        template <class T>
        struct StateHandle : public StateHandleBase
        {
          Managed<MutableState<T> > handle;
          explicit StateHandle(const Managed<MutableState<T> > &h) : handle(h) {}
          StateBase *state() const { return handle.get(); }
        };

        template <class T>
        MutableState<T> &useStateWithValue(const T &initial)
        {
          MutableState<T> *state = new MutableState<T>(initial);
          ownedStates_.push_back(state);
          tracker_.addState(state);
          return *state;
        }

        template <class T>
        Managed<MutableState<T> > useManagedStateWithValue(const T &initial)
        {
          Managed<MutableState<T> > handle = Managed<MutableState<T> >::Wrap(new MutableState<T>(initial));
          StateHandleBase *entry = new StateHandle<T>(handle);
          ownedStateHandles_.push_back(entry);
          tracker_.addState(entry->state());
          return handle;
        }

        void clearOwnedStates()
        {
          for (size_t i = 0; i < ownedStates_.size(); ++i)
          {
            delete ownedStates_[i];
          }
          ownedStates_.clear();
        }

        void clearOwnedStateHandles()
        {
          for (size_t i = 0; i < ownedStateHandles_.size(); ++i)
          {
            delete ownedStateHandles_[i];
          }
          ownedStateHandles_.clear();
        }

        declara::core::PushStateTracker tracker_;
        std::vector<StateBase *> ownedStates_;
        std::vector<StateHandleBase *> ownedStateHandles_;
      };

      template <class PropsT, class NodeT>
      struct BoundaryDefinition : public NodeDefinition<PropsT, NodeT>
      {
        typedef NodeDefinition<PropsT, NodeT> BaseType;
        BoundaryDefinition() : BaseType() {}
        BoundaryDefinition(const PropsT &p) : BaseType(p) {}
        virtual NodeDefinitionBase *clone() const { return new BoundaryDefinition(*this); }
        virtual bool isBoundary() const { return true; }
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // DECLARA_CORE2_SCENE_NODE_BOUNDARY_HPP
