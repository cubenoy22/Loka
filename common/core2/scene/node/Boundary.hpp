#ifndef LOKA_CORE2_SCENE_NODE_BOUNDARY_HPP
#define LOKA_CORE2_SCENE_NODE_BOUNDARY_HPP

#include <cstdarg>
#include <vector>
#include "../Node.hpp"
#include "ComposableNode.hpp"
#include "../BoundState.hpp"
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
      class Scene;

      // BoundaryNode: owns a local tracker for its subtree.
      class BoundaryNode : public ComposableNode, public IStateOwner
      {
      public:
        BoundaryNode() : ComposableNode(), tracker_(), scene_(0), parentBoundary_(0) {}
        virtual ~BoundaryNode()
        {
          clearOwnedStates();
          clearOwnedStateHandles();
        }

        virtual declara::core::StateTracker *tracker() { return &tracker_; }
        Scene *scene() const { return scene_; }
        Scene *getScene() const
        {
          if (scene_)
          {
            return scene_;
          }
          return parentBoundary_ ? parentBoundary_->getScene() : 0;
        }
        void setScene(Scene *scene) { scene_ = scene; }
        BoundaryNode *parentBoundary() const { return parentBoundary_; }
        void setParentBoundary(BoundaryNode *parent) { parentBoundary_ = parent; }

        template <class T>
        BoundState<T> useState()
        {
          return useStateWithValue(T());
        }

        template <class T>
        BoundState<T> useState(const T &initial)
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
        virtual void adoptState(StateBase *state)
        {
          if (!state)
          {
            return;
          }
          ownedStates_.push_back(state);
          tracker_.addState(state);
        }

        static void composeTree(Node *node, ComponentContext &parentContext, ComposeEvent event, BoundaryNode *currentBoundary)
        {
          if (!node)
          {
            return;
          }
          BoundaryNode *boundary = dynamic_cast<BoundaryNode *>(node);
          BoundaryNode *nextBoundary = currentBoundary;
          if (boundary)
          {
            boundary->setParentBoundary(currentBoundary);
            if (currentBoundary)
            {
              boundary->setScene(currentBoundary->getScene());
            }
            nextBoundary = boundary;
          }
          ComposableNode *composable = dynamic_cast<ComposableNode *>(node);
          ComponentContext *contextForChildren = &parentContext;
          ComponentContext nodeContext(&parentContext);
          nodeContext.setStateOwner(parentContext.stateOwner());
          nodeContext.setBoundary(nextBoundary);
          Scene *scene = nextBoundary ? nextBoundary->getScene() : 0;
          nodeContext.setScene(scene);
          nodeContext.setWindow(parentContext.window());
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
          Node *child = nestable->childrenHead();
          size_t count = nestable->childrenCount();
          size_t index = 0;
          while (child && index < count)
          {
            composeTree(child, *contextForChildren, event, nextBoundary);
            child = child->nextInComposition;
            ++index;
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
        BoundState<T> useStateWithValue(const T &initial)
        {
          MutableState<T> *state = new MutableState<T>(initial);
          adoptState(state);
          return BoundState<T>(state, this->tracker());
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
        Scene *scene_;
        BoundaryNode *parentBoundary_;
      };

      template <class PropsT, class NodeT>
      struct BoundaryDefinition : public NodeDefinition<PropsT, NodeT>
      {
        typedef NodeDefinition<PropsT, NodeT> BaseType;
        typedef int IsBoundaryDefinition;
        BoundaryDefinition() : BaseType() {}
        BoundaryDefinition(const PropsT &p) : BaseType(p) {}
        virtual NodeDefinitionBase *clone() const { return new BoundaryDefinition(*this); }
        virtual bool isBoundary() const { return true; }
      };

    } // namespace scene
  } // namespace core
} // namespace declara

#endif // LOKA_CORE2_SCENE_NODE_BOUNDARY_HPP
