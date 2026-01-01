#ifndef DECLARA_CORE2_SCENE_NODE_BOUNDARY_HPP
#define DECLARA_CORE2_SCENE_NODE_BOUNDARY_HPP

#include <cstdarg>
#include <vector>
#include "../Node.hpp"
#include "ComposableNode.hpp"
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

      protected:
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
        template <class T>
        MutableState<T> &useStateWithValue(const T &initial)
        {
          MutableState<T> *state = new MutableState<T>(initial);
          ownedStates_.push_back(state);
          tracker_.addState(state);
          return *state;
        }

        void clearOwnedStates()
        {
          for (size_t i = 0; i < ownedStates_.size(); ++i)
          {
            delete ownedStates_[i];
          }
          ownedStates_.clear();
        }

        declara::core::PushStateTracker tracker_;
        std::vector<StateBase *> ownedStates_;
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
